#include "Stage/Testing/StageSoundLab.h"

#include "Animation/CharacterPhysicsAnimationSet.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/Physics/CharacterPhysicsReactionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/StageShiftCharacter.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/PhysicsActor.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "Sound/SoundBase.h"
#include "Stage/Environment/StageSoundControl.h"
#include "Stage/Environment/StageSpotlightControl.h"
#include "Stage/Environment/StageTurntable.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
void SetSound(UObject* Object, FName PropertyName, USoundBase* Sound)
{
	if (Object)
		if (const auto* Property = FindFProperty<FObjectPropertyBase>(Object->GetClass(), PropertyName))
			Property->SetObjectPropertyValue_InContainer(Object, Sound);
}
}

AStageSoundLab::AStageSoundLab()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AStageSoundLab::Setup(APlayerController& Controller)
{
	LocalController = &Controller;
	Player = Cast<AStageShiftCharacter>(Controller.GetPawn());
	SelectedTakes.Init(0, Cases.Num());
	for (const FStageSoundLabCase& Case : Cases)
	{
		if (Case.Target) InitialTransforms.Add(Case.Target.Get(), Case.Target->GetActorTransform());
		BindTake(Case, 0);
	}
	if (Parcel) InitialTransforms.Add(Parcel.Get(), Parcel->GetActorTransform());

	EnableInput(&Controller);
	InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &AStageSoundLab::NextCase).bConsumeInput = true;
	InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &AStageSoundLab::PreviousCase).bConsumeInput = true;
	InputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &AStageSoundLab::NextTake).bConsumeInput = true;
	InputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &AStageSoundLab::PreviousTake).bConsumeInput = true;
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AStageSoundLab::RunCurrent).bConsumeInput = true;
	InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &AStageSoundLab::ResetFixtures).bConsumeInput = true;
	InputComponent->BindKey(EKeys::F7, IE_Pressed, this, &AStageSoundLab::StopMusic).bConsumeInput = true;

	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Panel = SNew(SConstraintCanvas).Visibility(EVisibility::HitTestInvisible)
		+ SConstraintCanvas::Slot()
		.Anchors(FAnchors(0.f, 0.f)).Alignment(FVector2D::ZeroVector)
		.Offset(FMargin(26, 28, 418, 0)).AutoSize(true)
		[
			SNew(SBox).WidthOverride(418)
			[
				SNew(SBorder).Padding(FMargin(17, 13))
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.033f, 0.023f, 0.018f, 0.94f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Text_UObject(this, &AStageSoundLab::GetHeading)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 19))
						.ColorAndOpacity(FLinearColor(1.f, .79f, .51f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 0)
					[
						SNew(STextBlock).Text_UObject(this, &AStageSoundLab::GetTakeLabel)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
					[
						SNew(STextBlock).Text_UObject(this, &AStageSoundLab::GetHint).AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 9, 0, 0)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("PgUp/PgDn  Test    [ / ]  Sound    Enter  Run\nF6  Reset props    F7  Stop music")))
						.ColorAndOpacity(FLinearColor(.78f, .7f, .61f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
					[
						SNew(STextBlock).Text_UObject(this, &AStageSoundLab::GetResult).AutoWrapText(true)
						.ColorAndOpacity(FLinearColor(1.f, .79f, .51f))
					]
				]
			]
		];
		Viewport->AddViewportWidgetContent(Panel.ToSharedRef(), 20);
	}
}

void AStageSoundLab::EndPlay(const EEndPlayReason::Type Reason)
{
	StopMusic();
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport(); Viewport && Panel.IsValid())
		Viewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
	Panel.Reset();
	Super::EndPlay(Reason);
}

void AStageSoundLab::SelectCase(int32 Direction)
{
	if (Cases.IsEmpty()) return;
	CaseIndex = (CaseIndex + Direction + Cases.Num()) % Cases.Num();
	GetWorldTimerManager().ClearTimer(ResultTimer);
	Result.Reset();
}

void AStageSoundLab::SelectTake(int32 Direction)
{
	if (!SelectedTakes.IsValidIndex(CaseIndex)) return;
	const FStageSoundLabCase& Case = Cases[CaseIndex];
	const int32 Count = Case.Takes.Num();
	if (!Count) return;
	int32& Index = SelectedTakes[CaseIndex];
	Index = (Index + Direction + Count) % Count;
	BindTake(Case, Index);
	Result.Reset();
}

void AStageSoundLab::BindTake(const FStageSoundLabCase& Case, int32 Take) const
{
	if (!Player.IsValid() || !Case.Takes.IsValidIndex(Take)) return;
	USoundBase* Sound = Case.Takes[Take];
	switch (Case.Action)
	{
	case EStageSoundLabAction::Walk: SetSound(Player.Get(), TEXT("FootstepSound"), Sound); break;
	case EStageSoundLabAction::Land: SetSound(Player.Get(), TEXT("LandSound"), Sound); break;
	case EStageSoundLabAction::Fall:
	case EStageSoundLabAction::Stumble: break;
	case EStageSoundLabAction::Grab: SetSound(Player->FindComponentByClass<UPhysicsManipulationComponent>(), TEXT("GrabSound"), Sound); break;
	case EStageSoundLabAction::Release: SetSound(Player->FindComponentByClass<UPhysicsManipulationComponent>(), TEXT("ReleaseSound"), Sound); break;
	case EStageSoundLabAction::Throw: SetSound(Player->FindComponentByClass<UPhysicsManipulationComponent>(), TEXT("ThrowSound"), Sound); break;
	case EStageSoundLabAction::Catch:
		if (Receiver) SetSound(Receiver->FindComponentByClass<UPhysicsManipulationComponent>(), TEXT("GrabSound"), Sound);
		break;
	case EStageSoundLabAction::Impact: SetSound(Case.Target, TEXT("ImpactSound"), Sound); break;
	case EStageSoundLabAction::Cue: SetSound(Case.Target, TEXT("Sound"), Sound); break;
	case EStageSoundLabAction::Spotlight: SetSound(Case.Target, TEXT("ActivationSound"), Sound); break;
	case EStageSoundLabAction::RotationStart: SetSound(Case.Target, TEXT("RotationSound"), Sound); break;
	case EStageSoundLabAction::RotationLoop: SetSound(Case.Target, TEXT("RotationLoopSound"), Sound); break;
	case EStageSoundLabAction::RotationStop: SetSound(Case.Target, TEXT("RotationStopSound"), Sound); break;
	default: break;
	}
}

void AStageSoundLab::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!LocalController.IsValid())
	{
		APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0);
		if (Controller && Controller->IsLocalController() && Cast<AStageShiftCharacter>(Controller->GetPawn())) Setup(*Controller);
	}
	if (!Player.IsValid()) return;

	if (WalkLeft > 0.f)
	{
		WalkLeft -= DeltaSeconds;
		Player->AddMovementInput(WalkDirection);
	}
	if (InteractionLeft > 0.f)
	{
		InteractionLeft -= DeltaSeconds;
		if (auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalController->GetLocalPlayer()); Input && InteractionAction)
			Input->InjectInputForAction(InteractionAction, FInputActionValue(true), {}, {});
	}
	if (AActor* Actor = MovingFixture.Get())
	{
		MoveElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(MoveElapsed / MoveDuration, 0.f, 1.f);
		Actor->SetActorLocation(FMath::Lerp(MoveFrom, MoveTo, FMath::SmoothStep(0.f, 1.f, Alpha)),
		    false, nullptr, ETeleportType::TeleportPhysics);
		if (Alpha >= 1.f)
		{
			if (USoundBase* Sound = ArrivalSound.Get()) UGameplayStatics::PlaySoundAtLocation(this, Sound, MoveTo);
			MovingFixture.Reset();
		}
	}
	if (AActor* Actor = Curtain.Get(); Actor && !FMath::IsNearlyEqual(CurtainOpen, CurtainGoal))
	{
		CurtainOpen = FMath::FInterpConstantTo(CurtainOpen, CurtainGoal, DeltaSeconds, .72f);
		if (auto* Mesh = Actor->FindComponentByClass<USkeletalMeshComponent>())
			Mesh->SetMorphTarget(TEXT("Open"), CurtainOpen);
	}
}

void AStageSoundLab::PrepareInteraction(AStageShiftCharacter& Pawn, EStageSoundLabAction Action)
{
	auto* Manipulation = Pawn.FindComponentByClass<UPhysicsManipulationComponent>();
	if (!Manipulation || !Parcel || InteractionLeft > 0.f) return;
	if (Manipulation->IsCarrying())
	{
		if (Action == EStageSoundLabAction::Grab)
		{
			Result = TEXT("Already carrying. F6 resets the parcel.");
			return;
		}
		InteractionLeft = Action == EStageSoundLabAction::Throw ? .6f : .08f;
		Result = Action == EStageSoundLabAction::Throw ? TEXT("Charging the throw...") : TEXT("Releasing the parcel...");
		return;
	}
	if (Manipulation->GetManipulatedComponent()) return;

	Restore(*Parcel);
	const FVector PropLocation = Parcel->GetActorLocation();
	const FVector Approach = PropLocation - FVector(87.f, 0.f, 0.f);
	UCharacterMovementComponent* Movement = Pawn.GetCharacterMovement();
	Movement->StopMovementImmediately();
	const float GroundedZ = Pawn.GetActorLocation().Z;
	Pawn.SetActorLocationAndRotation(FVector(Approach.X, Approach.Y, GroundedZ), FRotator::ZeroRotator,
	    false, nullptr, ETeleportType::TeleportPhysics);
	if (APlayerController* Controller = LocalController.Get()) Controller->SetControlRotation(FRotator::ZeroRotator);
	InteractionLeft = .08f;
	Result = Action == EStageSoundLabAction::Grab ? TEXT("Picking up the parcel...") : TEXT("Picking up first. Press Enter again once it is in hand.");
}

void AStageSoundLab::Restore(AStagePhysicsActor& Prop) const
{
	const FTransform* Start = InitialTransforms.Find(&Prop);
	if (!Start) return;
	UStaticMeshComponent* Body = Prop.GetStaticMeshComponent();
	Body->SetSimulatePhysics(true);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Prop.SetActorTransform(*Start, false, nullptr, ETeleportType::ResetPhysics);
}

void AStageSoundLab::ResetFixtures()
{
	if (Player.IsValid())
		if (auto* Interaction = Player->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
	if (Receiver)
		if (auto* Manipulation = Receiver->FindComponentByClass<UPhysicsManipulationComponent>()) Manipulation->ReleaseComponent();
	InteractionLeft = WalkLeft = 0.f;
	MovingFixture.Reset();
	for (const auto& Entry : InitialTransforms)
	{
		if (auto* Prop = Cast<AStagePhysicsActor>(Entry.Key.Get()); Prop && !Prop->IsBeingManipulated()) Restore(*Prop);
		else if (AActor* Actor = Entry.Key.Get(); Actor && !Cast<AStagePhysicsActor>(Actor)
		    && !Cast<AStageTurntable>(Actor) && !Cast<APawn>(Actor))
			Actor->SetActorTransform(Entry.Value, false, nullptr, ETeleportType::TeleportPhysics);
	}
	Result = TEXT("Fixtures reset.");
}

void AStageSoundLab::MoveFixture(AActor& Target, const FVector& Destination, float Seconds, USoundBase* Sound)
{
	MovingFixture = &Target;
	MoveFrom = Target.GetActorLocation();
	MoveTo = Destination;
	MoveElapsed = 0.f;
	MoveDuration = Seconds;
	ArrivalSound = Sound;
}

void AStageSoundLab::RunCurrent()
{
	AStageShiftCharacter* Pawn = Player.Get();
	if (!Pawn || !SelectedTakes.IsValidIndex(CaseIndex)) return;
	const FStageSoundLabCase& Case = Cases[CaseIndex];
	if (!Case.Takes.IsValidIndex(SelectedTakes[CaseIndex])) return;
	USoundBase* Sound = Case.Takes[SelectedTakes[CaseIndex]];
	AActor* Target = Case.Target;
	const FVector Location = Target ? Target->GetActorLocation() : Pawn->GetActorLocation();
	auto PlayHere = [this, Sound, Location] { UGameplayStatics::PlaySoundAtLocation(this, Sound, Location); };
	Result.Reset();

	switch (Case.Action)
	{
	case EStageSoundLabAction::Walk:
	case EStageSoundLabAction::Fabric:
		WalkDirection = Pawn->GetActorLocation().Y < 0.f ? FVector::RightVector : -FVector::RightVector;
		WalkLeft = .65f;
		if (Case.Action == EStageSoundLabAction::Fabric)
			GetWorldTimerManager().SetTimer(ResultTimer, FTimerDelegate::CreateWeakLambda(this, PlayHere), .16f, false);
		break;
	case EStageSoundLabAction::Land: Pawn->Jump(); break;
	case EStageSoundLabAction::Fall:
	case EStageSoundLabAction::Stumble:
	{
		auto* Physics = Pawn->FindComponentByClass<UPhysicalMontageComponent>();
		auto* Reaction = Pawn->FindComponentByClass<UCharacterPhysicsReactionComponent>();
		if (!Physics || !Reaction || Physics->IsPhysicsMontageActive())
		{
			Result = TEXT("Wait for the doll to finish recovering.");
			break;
		}
		const bool bStumble = Case.Action == EStageSoundLabAction::Stumble;
		SetSound(Physics, TEXT("FallSound"), Case.FallImpactSound ? nullptr : Sound);
		SetSound(Physics, TEXT("FallImpactSound"), Case.FallImpactSound);
		SetSound(Reaction, TEXT("StumbleSound"), bStumble && Case.FallImpactSound ? Sound : nullptr);
		FPhysicalAnimationInput Input;
		Input.LaunchDirection = Pawn->GetActorForwardVector();
		Input.InitialVelocity = Input.LaunchDirection * 120.f;
		const bool bStarted = Reaction->PlayReaction(bStumble ? ECharacterPhysicsReaction::Stumble : ECharacterPhysicsReaction::Impact,
		    Input.LaunchDirection, Input);
		Result = bStarted ? TEXT("Recover before repeating.") : TEXT("Could not start the physics reaction.");
		break;
	}
	case EStageSoundLabAction::Grab:
	case EStageSoundLabAction::Release:
	case EStageSoundLabAction::Throw: PrepareInteraction(*Pawn, Case.Action); break;
	case EStageSoundLabAction::Impact:
		if (auto* Prop = Cast<AStagePhysicsActor>(Target); Prop && !Prop->IsBeingManipulated())
		{
			Restore(*Prop);
			Prop->AddActorWorldOffset(FVector(0.f, 0.f, 92.f), false, nullptr, ETeleportType::ResetPhysics);
		}
		break;
	case EStageSoundLabAction::Cue:
		if (auto* Control = Cast<AStageSoundControl>(Target))
		{
			Control->CancelWindow();
			Control->OpenWindow();
			Control->PlaySignal();
		}
		break;
	case EStageSoundLabAction::Spotlight:
		if (auto* Light = Cast<AStageSpotlightControl>(Target))
		{
			FInteractionContext Context;
			Context.Interactor = Pawn;
			if (!Light->CanInteract_Implementation(Context))
				Pawn->SetActorLocation(FVector(Location.X - 95.f, Location.Y, Pawn->GetActorLocation().Z),
				    false, nullptr, ETeleportType::TeleportPhysics);
			Light->OnInteraction_Implementation(Context);
		}
		break;
	case EStageSoundLabAction::RotationStart:
	case EStageSoundLabAction::RotationLoop:
	case EStageSoundLabAction::RotationStop:
		if (auto* Table = Cast<AStageTurntable>(Target)) Table->RotateNext();
		break;
	case EStageSoundLabAction::CurtainOpen:
	case EStageSoundLabAction::CurtainClose:
		Curtain = Target;
		CurtainGoal = Case.Action == EStageSoundLabAction::CurtainOpen ? 1.f : 0.f;
		CurtainOpen = 1.f - CurtainGoal;
		PlayHere();
		break;
	case EStageSoundLabAction::Catch:
		if (auto* Prop = Cast<AStagePhysicsActor>(Target); Prop && Receiver)
		{
			auto* Manipulation = Receiver->FindComponentByClass<UPhysicsManipulationComponent>();
			if (!Manipulation || !Manipulation->GetCarryPoint()) break;
			Manipulation->ReleaseComponent();
			Manipulation->SetAcceptStageProps(true);
			if (!Manipulation->CanReceiveStageProp() || Prop->IsBeingManipulated()) break;

			constexpr float FlightTime = .45f;
			const FVector Hand = Manipulation->GetCarryPoint()->GetComponentLocation();
			const FVector Start = Hand + Receiver->GetActorForwardVector() * 220.f + FVector::UpVector * 33.f;
			Restore(*Prop);
			Prop->SetActorLocation(Start, false, nullptr, ETeleportType::ResetPhysics);
			Prop->GetStaticMeshComponent()->SetPhysicsLinearVelocity((Hand - Start) / FlightTime
			    - FVector::UpVector * (GetWorld()->GetGravityZ() * .5f * FlightTime));
			Prop->WatchThrownProp(Pawn);
		}
		break;
	case EStageSoundLabAction::Place:
		if (Target)
			if (const FTransform* Start = InitialTransforms.Find(Target))
			{
				const FVector Mark = Start->GetLocation();
				Target->SetActorLocation(Mark + FVector(-35.f, 0.f, 45.f));
				MoveFixture(*Target, Mark, .36f, Sound);
			}
		break;
	case EStageSoundLabAction::Paper:
	case EStageSoundLabAction::Slide:
		if (Target)
			if (const FTransform* Start = InitialTransforms.Find(Target))
			{
				const FVector Base = Start->GetLocation();
				const FVector Offset = Case.Action == EStageSoundLabAction::Paper ? FVector(0.f, -15.f, 0.f) : FVector(0.f, -75.f, 0.f);
				const FVector End = FVector::DistSquared(Location, Base) < 25.f ? Base + Offset : Base;
				MoveFixture(*Target, End, Case.Action == EStageSoundLabAction::Paper ? .24f : .75f);
				PlayHere();
			}
		break;
	case EStageSoundLabAction::UIUrgent:
		Result = TEXT("Waiting for the cue deadline...");
		GetWorldTimerManager().SetTimer(ResultTimer, FTimerDelegate::CreateWeakLambda(this, [this, Sound]
		{
			Result = TEXT("Cue overdue");
			UGameplayStatics::PlaySound2D(this, Sound);
		}), 1.2f, false);
		break;
	case EStageSoundLabAction::Music:
		StopMusic();
		Music = UGameplayStatics::SpawnSound2D(this, Sound);
		Result = TEXT("Music playing. Use F7 to stop.");
		break;
	default:
		if (Case.Action == EStageSoundLabAction::Applause) Result = TEXT("Performance complete");
		else if (Case.Action == EStageSoundLabAction::UIHover) Result = TEXT("Cue highlighted");
		else if (Case.Action == EStageSoundLabAction::UIPress) Result = TEXT("Cue pressed");
		else if (Case.Action == EStageSoundLabAction::UIConfirm) Result = TEXT("Cue confirmed");
		else if (Case.Action == EStageSoundLabAction::UIBack) Result = TEXT("Returned to the previous cue");
		UGameplayStatics::PlaySound2D(this, Sound);
		break;
	}
}

void AStageSoundLab::StopMusic()
{
	if (UAudioComponent* Audio = Music.Get()) Audio->Stop();
	Music.Reset();
}

FText AStageSoundLab::GetHeading() const
{
	return Cases.IsValidIndex(CaseIndex)
	    ? FText::FromString(FString::Printf(TEXT("%02d/%02d  %s"), CaseIndex + 1, Cases.Num(), *Cases[CaseIndex].Label))
	    : FText::FromString(TEXT("Sound Lab"));
}

FText AStageSoundLab::GetTakeLabel() const
{
	if (!SelectedTakes.IsValidIndex(CaseIndex)) return FText::GetEmpty();
	const auto& Takes = Cases[CaseIndex].Takes;
	const int32 Index = SelectedTakes[CaseIndex];
	if (!Takes.IsValidIndex(Index) || !Takes[Index]) return FText::GetEmpty();
	return FText::FromString(FString::Printf(TEXT("%d/%d  %s"), Index + 1, Takes.Num(), *Takes[Index]->GetName()));
}

FText AStageSoundLab::GetHint() const
{
	return Cases.IsValidIndex(CaseIndex) ? FText::FromString(Cases[CaseIndex].Hint) : FText::GetEmpty();
}

FText AStageSoundLab::GetResult() const
{
	return FText::FromString(Result);
}
