#include "Stage/Environment/StageRehearsal.h"

#include "Components/CapsuleComponent.h"
#include "Components/Customization/StageShiftCharacterPreset.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/StageMemberCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TriggerBox.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "Stage/Cues/StageCueComponent.h"
#include "Stage/Cues/StageCueWorldMarker.h"
#include "Stage/Environment/StageCurtainRail.h"
#include "Stage/Environment/StageTurntable.h"
#include "Stage/Environment/StageTurntableControl.h"
#include "TimerManager.h"
#include "UI/StageInputDeviceSubsystem.h"
#include "UI/StageCueWidget.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "StageRehearsal"

namespace
{
const FName RehearsalCueId(TEXT("HubRehearsal"));
constexpr float RehearsalMoveRadius = 56.f;
constexpr float RehearsalPlaceRadius = 75.f;

bool IsNearRehearsalMark(const AActor& Actor, const AActor& Mark, float Radius)
{
	return FVector::DistSquared2D(Actor.GetActorLocation(), Mark.GetActorLocation()) <= FMath::Square(Radius);
}
}

AStageRehearsal::AStageRehearsal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	ReplayStand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReplayStand"));
	SetRootComponent(ReplayStand);
	ReplayStand->SetCollisionProfileName(TEXT("BlockAll"));
	ReplayStand->CanCharacterStepUpOn = ECB_No;
	Cues = CreateDefaultSubobject<UStageCueComponent>(TEXT("RehearsalCues"));
	Cues->bUseWorldSignals = false;
	Cues->MaxCards = 1;
	static ConstructorHelpers::FObjectFinder<UInputAction> Action(TEXT("/Game/DEV/Core/Input/Inputs/IA_Interaction"));
	InteractionAction = Action.Object;
}

bool AStageRehearsal::IsConfigured() const
{
	return IsValid(Room) && IsValid(Entrance) && IsValid(MoveMark) && IsValid(PlaceMark) && IsValid(Parcel) && IsValid(Crate)
	    && IsValid(Curtain) && IsValid(TurntableControl) && IsValid(TurntableControl->Turntable) && IsValid(SoundControl)
	    && SoundControl->HasSound(KnockSoundId) && PerformerClass && IsValid(PerformerStart) && IsValid(PerformerMark)
	    && Cues->WidgetClass && Cues->MarkerClass;
}

void AStageRehearsal::BeginPlay()
{
	Super::BeginPlay();
	if (!ensureMsgf(IsConfigured(), TEXT("Rehearsal has missing room, cast, mechanism or UI bindings"))) return;
	SaveSlot = GetWorld()->WorldType == EWorldType::PIE ? TEXT("StageShift_Rehearsal_PIE") : TEXT("StageShift_Rehearsal");
	if (UGameplayStatics::DoesSaveGameExist(SaveSlot, 0))
		Progress = Cast<UStageRehearsalProgress>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (!Progress) Progress = NewObject<UStageRehearsalProgress>(this);
	ParcelHome = Parcel->GetActorTransform();
	RoomBounds = Room->GetComponentsBoundingBox();
	Room->OnActorBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnEnter);
	Room->OnActorEndOverlap.AddUniqueDynamic(this, &ThisClass::OnExit);
	Parcel->OnPropTransfer.AddUniqueDynamic(this, &ThisClass::OnParcelTransfer);
	Parcel->GetStaticMeshComponent()->SetNotifyRigidBodyCollision(true);
	Parcel->GetStaticMeshComponent()->OnComponentHit.AddUniqueDynamic(this, &ThisClass::OnParcelHit);
	SoundControl->OnCueResult.AddUniqueDynamic(this, &ThisClass::OnSound);
	TurntableControl->SetControlEnabled(false);
	InitializeTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::InitializePlayer);
}

void AStageRehearsal::InitializePlayer()
{
	if (!bReady)
	{
		bReady = true;
		Curtain->ResetPracticePosition(true);
	}
	APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
	APawn* Pawn = Controller && Controller->IsLocalController() ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		GetWorldTimerManager().SetTimer(InitializeTimer, this, &ThisClass::InitializePlayer, .2f, false);
		return;
	}
	if (Room->IsOverlappingActor(Pawn))
	{
		OnEnter(Room, Pawn);
		return;
	}
	if (!Progress->bInvitationSeen && !Progress->bCompleted)
	{
		FStageCueDefinition Cue;
		Cue.CueId = RehearsalCueId;
		Cue.Title = LOCTEXT("InvitationTitle", "Загляни на репетицию");
		Cue.Details = LOCTEXT("InvitationDetails", "Вход справа от звукового пульта.");
		FStageCueTarget& Target = Cue.Targets.AddDefaulted_GetRef();
		Target.Type = EStageCueMarkerType::Control;
		Cues->ShowCueForActor(Cue, Entrance);
		Progress->bInvitationSeen = true;
		SaveProgress();
	}
}

void AStageRehearsal::SaveProgress()
{
	if (!UGameplayStatics::SaveGameToSlot(Progress, SaveSlot, 0))
		UE_LOG(LogTemp, Warning, TEXT("Could not save the hub rehearsal progress to %s"), *SaveSlot);
}

void AStageRehearsal::OnEnter(AActor*, AActor* Actor)
{
	APawn* Pawn = Cast<APawn>(Actor);
	auto* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!bReady || Player.IsValid() || !Controller || !Controller->IsLocalController()) return;
	Player = Pawn;
	if (ULocalPlayer* Local = Controller->GetLocalPlayer())
	{
		InputDevice = Local->GetSubsystem<UStageInputDeviceSubsystem>();
		if (InputDevice.IsValid()) InputDevice->OnInputDeviceChanged.AddUObject(this, &ThisClass::OnInputDeviceChanged);
	}
	if (Progress->bCompleted) SetStep(ERehearsalStep::Available);
	else StartPractice();
}

void AStageRehearsal::OnExit(AActor*, AActor* Actor)
{
	if (Player.Get() != Actor) return;
	SetStep(ERehearsalStep::Outside);
	ResetPractice();
	if (InputDevice.IsValid()) InputDevice->OnInputDeviceChanged.RemoveAll(this);
	InputDevice.Reset();
	Player.Reset();
}

const UMeshComponent* AStageRehearsal::GetInteractionMesh() const
{
	return ReplayStand;
}

bool AStageRehearsal::CanInteract_Implementation(const FInteractionContext& Context) const
{
	return Player.IsValid() && Context.Interactor == Player.Get()
	    && (Step == ERehearsalStep::Available || Step == ERehearsalStep::Complete);
}

EInteractionResult AStageRehearsal::OnInteraction_Implementation(const FInteractionContext& Context)
{
	if (Context.InteractionPhase != EInteractionPhase::Start || Context.bCancelled || !CanInteract_Implementation(Context))
		return EInteractionResult::Rejected;
	StartPractice();
	return EInteractionResult::Completed;
}

void AStageRehearsal::StartPractice()
{
	ResetPractice();
	SetStep(ERehearsalStep::Move);
}

void AStageRehearsal::ReleasePracticeInteraction() const
{
	if (APawn* Pawn = Player.Get())
	{
		const auto* Physics = Pawn->FindComponentByClass<UPhysicsManipulationComponent>();
		const auto* Held = Physics ? Physics->GetManipulatedComponent() : nullptr;
		if (Held && (Held->GetOwner() == Parcel || Held->GetOwner() == Curtain->GetLever()))
			if (auto* Interaction = Pawn->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
	}
}

void AStageRehearsal::ResetPractice()
{
	ReleasePracticeInteraction();
	SoundControl->CancelWindow();
	if (IsValid(Performer)) Performer->Destroy();
	Performer = nullptr;
	TurntableControl->Turntable->ResetPracticePosition();
	Curtain->ResetPracticePosition(true);
	ResetParcel();
	Cues->ResetCues();
}

void AStageRehearsal::ResetParcel()
{
	UStaticMeshComponent* Mesh = Parcel->GetStaticMeshComponent();
	Mesh->SetWorldTransform(ParcelHome, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Mesh->WakeAllRigidBodies();
	ThrowAt = -1.0;
	bHitCrate = false;
}

bool AStageRehearsal::IsHoldingParcel() const
{
	const APawn* Pawn = Player.Get();
	const auto* Physics = Pawn ? Pawn->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	return Physics && Physics->IsCarrying() && Physics->GetManipulatedComponent() == Parcel->GetStaticMeshComponent();
}

void AStageRehearsal::SetStep(ERehearsalStep Next)
{
	Step = Next;
	const bool bFreePractice = Step == ERehearsalStep::Available || Step == ERehearsalStep::Complete;
	Curtain->SetPracticeControlEnabled(bFreePractice || Step == ERehearsalStep::CloseCurtain || Step == ERehearsalStep::OpenCurtain);
	TurntableControl->SetControlEnabled(bFreePractice || Step == ERehearsalStep::Turn);
	SetActorTickEnabled(Step != ERehearsalStep::Outside);
	bShowingThrowTarget = Step == ERehearsalStep::Throw && IsHoldingParcel();
	RefreshPrompt();
}

void AStageRehearsal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Player.IsValid()) return;
	const bool bHeld = IsHoldingParcel();
	if (!bHeld && !RoomBounds.ExpandBy(35.f).IsInsideOrOn(Parcel->GetActorLocation())) ResetParcel();
	switch (Step)
	{
	case ERehearsalStep::Move:
		if (IsNearRehearsalMark(*Player.Get(), *MoveMark, RehearsalMoveRadius)) SetStep(ERehearsalStep::PickUp);
		break;
	case ERehearsalStep::PickUp:
		if (bHeld) SetStep(ERehearsalStep::Place);
		break;
	case ERehearsalStep::Place:
		if (!bHeld && IsNearRehearsalMark(*Parcel, *PlaceMark, RehearsalPlaceRadius)
		    && FMath::Abs(Parcel->GetActorLocation().Z - PlaceMark->GetActorLocation().Z) < 35.f
		    && Parcel->GetVelocity().SizeSquared() < FMath::Square(45.f)) SetStep(ERehearsalStep::Throw);
		break;
	case ERehearsalStep::Throw: UpdateThrow(); break;
	case ERehearsalStep::CloseCurtain:
		if (Curtain->GetOpenProgress() <= 0.f) SetStep(ERehearsalStep::Turn);
		break;
	case ERehearsalStep::Turn:
		if (!TurntableControl->Turntable->IsRotating() && TurntableControl->Turntable->GetCurrentSector() == 1)
		{
			StartPerformer();
			if (IsValid(Performer)) SetStep(ERehearsalStep::OpenCurtain);
		}
		break;
	case ERehearsalStep::OpenCurtain:
		if (Curtain->IsOpen()) SetStep(ERehearsalStep::PerformerWalk);
		break;
	case ERehearsalStep::PerformerWalk: WalkPerformer(); break;
	case ERehearsalStep::Outro:
		if (GetWorld()->GetTimeSeconds() >= FinishAt) SetStep(ERehearsalStep::Complete);
		break;
	default: break;
	}
}

void AStageRehearsal::OnParcelTransfer(AStagePhysicsActor*, AActor* From, AActor*, EStagePropTransferResult Result, float)
{
	if (Step == ERehearsalStep::Throw && From == Player.Get() && Result == EStagePropTransferResult::Thrown)
	{
		ThrowAt = GetWorld()->GetTimeSeconds();
		bHitCrate = false;
	}
}

void AStageRehearsal::OnParcelHit(UPrimitiveComponent*, AActor* Other, UPrimitiveComponent*, FVector, const FHitResult&)
{
	if (Step == ERehearsalStep::Throw && ThrowAt >= 0.0 && Other == Crate) bHitCrate = true;
}

void AStageRehearsal::UpdateThrow()
{
	const bool bHeld = IsHoldingParcel();
	if (bHeld)
	{
		ThrowAt = -1.0;
		bHitCrate = false;
	}
	if (ThrowAt >= 0.0)
	{
		FVector Center, Extent;
		Crate->GetActorBounds(false, Center, Extent);
		const FVector ParcelCenter = Parcel->GetStaticMeshComponent()->Bounds.Origin;
		const bool bInCrate = FBox(Center - Extent * .85, Center + Extent * .85).IsInsideOrOn(ParcelCenter);
		if (bHitCrate || bInCrate)
		{
			SetStep(ERehearsalStep::CloseCurtain);
			return;
		}
		const double Elapsed = GetWorld()->GetTimeSeconds() - ThrowAt;
		if (Elapsed > 4.0 || (Elapsed > 1.6 && Parcel->GetVelocity().SizeSquared() < FMath::Square(35.f))) ResetParcel();
	}
	if (bShowingThrowTarget != bHeld)
	{
		bShowingThrowTarget = bHeld;
		RefreshPrompt();
	}
}

void AStageRehearsal::StartPerformer()
{
	if (IsValid(Performer)) return;
	const auto* Default = PerformerClass->GetDefaultObject<AStageMemberCharacter>();
	const FVector Location = PerformerStart->GetActorLocation() + FVector(0, 0, Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Performer = GetWorld()->SpawnActor<AStageMemberCharacter>(PerformerClass, Location, PerformerStart->GetActorRotation(), Params);
	if (!IsValid(Performer)) return;
	if (PerformerAppearance)
	{
		FStageShiftCharacterAppearance Appearance = Performer->GetAppearance();
		Appearance.Hairstyle = PerformerAppearance->Hairstyle;
		Appearance.Hat = PerformerAppearance->Hat;
		Appearance.Outfit = PerformerAppearance->Outfit;
		Performer->ApplyAppearance(Appearance);
	}
	Performer->bUseControllerRotationYaw = false;
	UCharacterMovementComponent* Movement = Performer->GetCharacterMovement();
	Movement->bRunPhysicsWithNoController = true;
	Movement->bOrientRotationToMovement = true;
	Movement->MaxWalkSpeed = 106.f;
	Movement->SetMovementMode(MOVE_Walking);
}

void AStageRehearsal::WalkPerformer()
{
	if (!IsValid(Performer)) return;
	const FVector Delta = PerformerMark->GetActorLocation() - Performer->GetActorLocation();
	if (Delta.SizeSquared2D() > FMath::Square(13.f))
	{
		Performer->AddMovementInput(Delta.GetSafeNormal2D(), FMath::Clamp(Delta.Size2D() / 42.f, .35f, 1.f), true);
		return;
	}
	Performer->GetCharacterMovement()->StopMovementImmediately();
	Performer->SetActorRotation(PerformerMark->GetActorRotation());
	FStageSoundTiming Timing;
	Timing.bUntimed = true;
	if (SoundControl->OpenSoundWindow(KnockSoundId, Performer, Timing)) SetStep(ERehearsalStep::Sound);
}

void AStageRehearsal::OnSound(const FStageSoundWindow& Window, EStageSoundResult Result)
{
	if (Step != ERehearsalStep::Sound || Window.SoundId != KnockSoundId || Result != EStageSoundResult::OnTime) return;
	Progress->bCompleted = true;
	Progress->bInvitationSeen = true;
	SaveProgress();
	FinishAt = GetWorld()->GetTimeSeconds() + 2.0;
	SetStep(ERehearsalStep::Outro);
}

FText AStageRehearsal::InteractionKey() const
{
	const APawn* Pawn = Player.Get();
	const auto* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const auto* Local = Controller ? Controller->GetLocalPlayer() : nullptr;
	const auto* Input = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	const EStageInputDevice Device = InputDevice.IsValid() ? InputDevice->GetInputDevice() : EStageInputDevice::KeyboardMouse;
	if (Input && InteractionAction)
	{
		for (const FKey& Key : Input->QueryKeysMappedToAction(InteractionAction))
		{
			if (!Key.IsValid() || Key.IsGamepadKey() != (Device != EStageInputDevice::KeyboardMouse)) continue;
			if (Key == EKeys::LeftMouseButton) return LOCTEXT("Mouse", "ЛКМ");
			if (Key == EKeys::Gamepad_RightShoulder) return Device == EStageInputDevice::PlayStation ? LOCTEXT("R1", "R1") : LOCTEXT("RB", "RB");
			return Key.GetDisplayName(false);
		}
	}
	return LOCTEXT("Interact", "Взаимодействие");
}

FText AStageRehearsal::PromptDetails() const
{
	switch (Step)
	{
	case ERehearsalStep::Available:
		return FText::Format(LOCTEXT("ReplayDetails", "{0} у пюпитра, чтобы начать заново. Механизмы доступны для практики."), InteractionKey());
	case ERehearsalStep::Move:
		return InputDevice.IsValid() && InputDevice->GetInputDevice() != EStageInputDevice::KeyboardMouse
		    ? LOCTEXT("MoveStick", "Иди к метке левым стиком.") : LOCTEXT("MoveKeys", "Иди к метке с помощью WASD.");
	case ERehearsalStep::PickUp: return FText::Format(LOCTEXT("Take", "Нажми {0} у верстака."), InteractionKey());
	case ERehearsalStep::Place: return FText::Format(LOCTEXT("Drop", "Донеси свёрток до метки и коротко нажми {0}."), InteractionKey());
	case ERehearsalStep::Throw:
		return FText::Format(bShowingThrowTarget ? LOCTEXT("Aim", "Удерживай {0}, наведи прицел на ящик и отпусти.")
		    : LOCTEXT("TakeAgain", "Подбери свёрток через {0}, чтобы бросить его в ящик."), InteractionKey());
	case ERehearsalStep::CloseCurtain:
	case ERehearsalStep::OpenCurtain:
		return FText::Format(LOCTEXT("Slide", "Удерживай {0} и перетащи рукоятку до упора."), InteractionKey());
	case ERehearsalStep::Turn: return FText::Format(LOCTEXT("Turn", "Нажми {0} у круглого пульта и дождись остановки."), InteractionKey());
	case ERehearsalStep::Sound: return FText::Format(LOCTEXT("Knock", "Подойди к звуковому пульту и нажми {0}, чтобы подать «Стук»."), InteractionKey());
	case ERehearsalStep::Complete:
		return FText::Format(LOCTEXT("CompleteDetails", "Выход ведёт в хаб. Для повтора нажми {0} у пюпитра."), InteractionKey());
	default: return FText::GetEmpty();
	}
}

void AStageRehearsal::RefreshPrompt()
{
	Cues->DismissCue(RehearsalCueId);
	FStageCueDefinition Cue;
	Cue.CueId = RehearsalCueId;
	Cue.Details = PromptDetails();
	AActor* Target = nullptr;
	EStageCueMarkerType Marker = EStageCueMarkerType::Control;
	switch (Step)
	{
	case ERehearsalStep::Available: Cue.Title = LOCTEXT("Replay", "Повторить репетицию"); Target = this; break;
	case ERehearsalStep::Move: Cue.Title = LOCTEXT("Walk", "Подойди к метке"); Target = MoveMark; Marker = EStageCueMarkerType::Zone; break;
	case ERehearsalStep::PickUp:
		Cue.Title = LOCTEXT("Pickup", "Возьми свёрток"); Target = Parcel; Marker = EStageCueMarkerType::Object; break;
	case ERehearsalStep::Place:
		Cue.Title = LOCTEXT("Place", "Положи свёрток на метку"); Target = PlaceMark; Marker = EStageCueMarkerType::Zone; break;
	case ERehearsalStep::Throw:
		Cue.Title = LOCTEXT("Throw", "Попади свёртком в ящик");
		Target = bShowingThrowTarget ? Crate.Get() : Parcel.Get();
		Marker = bShowingThrowTarget ? EStageCueMarkerType::Control : EStageCueMarkerType::Object;
		break;
	case ERehearsalStep::CloseCurtain: Cue.Title = LOCTEXT("Close", "Закрой занавес"); Target = Curtain->GetLever(); break;
	case ERehearsalStep::Turn: Cue.Title = LOCTEXT("Rotate", "Поверни сцену"); Target = TurntableControl; break;
	case ERehearsalStep::OpenCurtain: Cue.Title = LOCTEXT("Open", "Открой занавес"); Target = Curtain->GetLever(); break;
	case ERehearsalStep::PerformerWalk: Cue.Title = LOCTEXT("ActorWalk", "Актёр идёт к двери"); break;
	case ERehearsalStep::Sound: Cue.Title = LOCTEXT("Sound", "Подай звуковой сигнал"); Target = SoundControl; break;
	case ERehearsalStep::Complete: Cue.Title = LOCTEXT("Complete", "Репетиция окончена"); Target = this; break;
	default: return;
	}
	if (Step >= ERehearsalStep::PickUp && Step <= ERehearsalStep::Throw) Cue.Kind = EStageCueKind::Props;
	if (Step >= ERehearsalStep::CloseCurtain && Step <= ERehearsalStep::OpenCurtain) Cue.Kind = EStageCueKind::Mechanism;
	if (Step == ERehearsalStep::Sound) Cue.Kind = EStageCueKind::Sound;
	if (Target)
	{
		FStageCueTarget& Definition = Cue.Targets.AddDefaulted_GetRef();
		Definition.Type = Marker;
		if (Marker == EStageCueMarkerType::Zone) Definition.Radius = Step == ERehearsalStep::Move ? RehearsalMoveRadius : RehearsalPlaceRadius;
	}
	Cues->ShowCueForActor(Cue, Target);
}

void AStageRehearsal::OnInputDeviceChanged(EStageInputDevice)
{
	Cues->SetCueDetails(RehearsalCueId, PromptDetails());
}

void AStageRehearsal::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(InitializeTimer);
	if (InputDevice.IsValid()) InputDevice->OnInputDeviceChanged.RemoveAll(this);
	if (IsValid(Room))
	{
		Room->OnActorBeginOverlap.RemoveDynamic(this, &ThisClass::OnEnter);
		Room->OnActorEndOverlap.RemoveDynamic(this, &ThisClass::OnExit);
	}
	if (IsValid(Parcel))
	{
		Parcel->OnPropTransfer.RemoveDynamic(this, &ThisClass::OnParcelTransfer);
		Parcel->GetStaticMeshComponent()->OnComponentHit.RemoveDynamic(this, &ThisClass::OnParcelHit);
	}
	if (IsValid(SoundControl)) SoundControl->OnCueResult.RemoveDynamic(this, &ThisClass::OnSound);
	Super::EndPlay(Reason);
}

#undef LOCTEXT_NAMESPACE
