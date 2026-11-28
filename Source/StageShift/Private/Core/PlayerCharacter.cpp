// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Core/PlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "StageShift/Public/Components/Interaction/InteractionComponent.h"
#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"
#include "Components/Interaction/ThrowAimComponent.h"
#include "Components/Interaction/InteractionOutlineComponent.h"
#include "Stage/Environment/StageBackstageShortcut.h"
#include "Stage/Environment/StageSelectionStation.h"
#include "Stage/Environment/StageSoundControl.h"
#include "UI/StageInteractionHintWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName DiveMontageName(TEXT("Jump"));
	const FName DiveHeadBone(TEXT("head"));
}

APlayerCharacter::APlayerCharacter()
{
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>("Interaction Component");
	InteractionOutlineComponent = CreateDefaultSubobject<UInteractionOutlineComponent>("Interaction Outline Component");
	PhysicsManipulationComponent = CreateDefaultSubobject<UPhysicsManipulationComponent>("Physics Manipulation Component");
	ThrowAimComponent = CreateDefaultSubobject<UThrowAimComponent>("Throw Aim Component");
	CarryPoint = CreateDefaultSubobject<USceneComponent>("Carry Point");
	CarryPoint->SetupAttachment(GetRootComponent());
	CarryPoint->SetRelativeLocation(FVector(75.0, 0.0, 25.0));
	PhysicsManipulationComponent->SetCarryPoint(CarryPoint);
	static ConstructorHelpers::FObjectFinder<UInputAction> CycleSound(
	    TEXT("/Game/DEV/Core/Input/Inputs/IA_CycleStageSound.IA_CycleStageSound"));
	CycleStageSoundInputAction = CycleSound.Object;
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	TargetSpeed = WalkSpeed;
	InteractionComponent->OnInteractionCandidateChanged.AddUObject(this, &ThisClass::OnInteractionCandidateChanged);

	PhysicsManipulationComponent->SetCarryPoint(CarryPoint);
	if (!CarrySocketName.IsNone() && GetMesh()->DoesSocketExist(CarrySocketName))
	{
		CarryPoint->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, CarrySocketName);
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()); PlayerController && PlayerController->IsLocalController())
	{
		// Keep the stereo orientation of the stage camera and measure distance from the worker.
		PlayerController->SetAudioListenerAttenuationOverride(GetRootComponent(), FVector::ZeroVector);
		if (!MappingContext) return;

		const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		check(IsValid(LocalPlayer));

		UEnhancedInputLocalPlayerSubsystem* EnhancedInputLocalPlayerSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		check(IsValid(EnhancedInputLocalPlayerSubsystem));

		EnhancedInputLocalPlayerSubsystem->AddMappingContext(MappingContext, 0);
	}
}

void APlayerCharacter::UnPossessed()
{
	RemoveInteractionHint();
	SetNearbyShortcut(nullptr);
	PreviousDiveHeadLocation.Reset();
	bTraversedShortcutDuringDive = false;
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()); PlayerController && PlayerController->IsLocalController())
	{
		PlayerController->ClearAudioListenerAttenuationOverride();
	}
	InteractionComponent->CancelInteraction();
	ThrowAimComponent->StopAiming();
	Super::UnPossessed();
	OnInteractionCandidateChanged(nullptr);
}

void APlayerCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	RemoveInteractionHint();
	Super::EndPlay(EndPlayReason);
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GetCharacterMovement()) return;
	UpdateBackstageShortcut();
	CheckDiveForBackstageShortcut();

	// Movement applies the mass-dependent Drag multiplier; sprint cannot bypass it.
	GetCharacterMovement()->MaxWalkSpeed =
	    PhysicsManipulationComponent->IsDragging() ? WalkSpeed : FMath::FInterpTo(GetCharacterMovement()->MaxWalkSpeed, TargetSpeed, DeltaSeconds, MovementBlendSpeed);
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input) return;

	if (MoveInputAction)
	{
		Input->BindAction(MoveInputAction, ETriggerEvent::Triggered, this, &ThisClass::OnMovementInput);
	}

	if (CameraInputAction)
	{
		Input->BindAction(CameraInputAction, ETriggerEvent::Triggered, this, &ThisClass::OnCameraInput);
		Input->BindAction(CameraInputAction, ETriggerEvent::Completed, this, &ThisClass::OnCameraInput);
		Input->BindAction(CameraInputAction, ETriggerEvent::Canceled, this, &ThisClass::OnCameraInput);
	}
	if (CycleStageSoundInputAction)
		Input->BindAction(CycleStageSoundInputAction, ETriggerEvent::Triggered, this, &ThisClass::OnCycleStageSound);

	InteractionComponent->BindInput(*Input, InteractInputAction);

	if (JumpInputAction)
	{
		Input->BindAction(JumpInputAction, ETriggerEvent::Started, this, &ThisClass::OnJumpInput);
	}

	if (SprintInputAction)
	{
		Input->BindAction(SprintInputAction, ETriggerEvent::Started, this, &ThisClass::OnSprintInput);
		Input->BindAction(SprintInputAction, ETriggerEvent::Completed, this, &ThisClass::OnSprintInput);
		Input->BindAction(SprintInputAction, ETriggerEvent::Canceled, this, &ThisClass::OnSprintInput);
	}
}

void APlayerCharacter::OnMovementInput(const FInputActionInstance& Instance)
{
	const FVector MovementInput = MovementInputToWorld(Instance.GetValue());
	AddMovementInput(MovementInput.GetSafeNormal(), MovementInput.Size());
}

FVector APlayerCharacter::MovementInputToWorld(const FInputActionValue& Value) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager) return FVector::ZeroVector;

	const FVector2D Axis = Value.Get<FVector2D>();

	const float CameraYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;

	const FRotationMatrix CameraBasis(FRotator(0.0f, CameraYaw, 0.0f));

	const FVector Forward = CameraBasis.GetUnitAxis(EAxis::X);
	const FVector Right = CameraBasis.GetUnitAxis(EAxis::Y);

	return Forward * Axis.X + Right * Axis.Y;
}

void APlayerCharacter::OnCameraInput(const FInputActionInstance& Instance)
{
	const bool bReleased = Instance.GetTriggerEvent() == ETriggerEvent::Completed || Instance.GetTriggerEvent() == ETriggerEvent::Canceled;
	const FVector2D Axis = bReleased ? FVector2D::ZeroVector : Instance.GetValue().Get<FVector2D>();
	if (auto* Console = FocusedSoundControl.Get(); Console && !InteractionComponent->HasActiveInteraction())
	{
		if (FMath::Abs(Axis.X) < 0.35) bSoundStickHeld = false;
		else if (!bSoundStickHeld && FMath::Abs(Axis.X) >= 0.6)
		{
			bSoundStickHeld = true;
			Console->CycleSound(Axis.X > 0.0 ? 1 : -1);
		}
		ThrowAimComponent->SetStickInput(FVector2D::ZeroVector);
		return;
	}
	bSoundStickHeld = false;
	ThrowAimComponent->SetStickInput(Axis);
}

void APlayerCharacter::OnCycleStageSound(const FInputActionValue& Value)
{
	const float Scroll = Value.Get<float>();
	if (auto* Console = FocusedSoundControl.Get(); Console && Scroll != 0.0f && !InteractionComponent->HasActiveInteraction())
		Console->CycleSound(Scroll > 0.0f ? 1 : -1);
}

void APlayerCharacter::OnInteractionCandidateChanged(AActor* Candidate)
{
	auto* Console = IsLocallyControlled() ? Cast<AStageSoundControl>(Candidate) : nullptr;
	UpdateInteractionHint(IsValid(Candidate) && Candidate->IsA<AStageSelectionStation>(), Console && Console->CanCycleSound());
	if (Console == FocusedSoundControl.Get()) return;
	if (auto* Previous = FocusedSoundControl.Get()) Previous->SetSelectorFocused(false);
	FocusedSoundControl = Console;
	bSoundStickHeld = false;
	if (Console) Console->SetSelectorFocused(true);
}

void APlayerCharacter::UpdateInteractionHint(bool bStageSelection, bool bSoundSelection)
{
	auto* Player = Cast<APlayerController>(GetController());
	if ((!bStageSelection && !bSoundSelection) || !Player || !Player->IsLocalController())
	{
		if (InteractionHint) InteractionHint->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	if (!InteractionHint)
	{
		const auto HintClass = GetClass()->GetDefaultObject<APlayerCharacter>()->InteractionHintClass;
		if (!HintClass) return;
		InteractionHint = CreateWidget<UStageInteractionHintWidget>(Player, HintClass);
		if (InteractionHint && !InteractionHint->AddToPlayerScreen())
			InteractionHint = nullptr;
	}
	if (InteractionHint) InteractionHint->Present(bSoundSelection);
}

void APlayerCharacter::RemoveInteractionHint()
{
	if (!InteractionHint) return;
	InteractionHint->RemoveFromParent();
	InteractionHint = nullptr;
}

void APlayerCharacter::OnJumpInput(const FInputActionInstance& Instance)
{
	if (PhysicsManipulationComponent->IsDragging() || !GetCharacterMovement()->IsMovingOnGround() || IsPhysicsMontageActive()) return;
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const UEnhancedPlayerInput* Input = PC ? Cast<UEnhancedPlayerInput>(PC->PlayerInput) : nullptr;
	const FVector Direction = Input && MoveInputAction ? MovementInputToWorld(Input->GetActionValue(MoveInputAction)).GetSafeNormal2D() : FVector::ZeroVector;
	PhysicalMontageComponent->StartPhysicsMontage(DiveMontageName, GetVelocity(), Direction);
}

void APlayerCharacter::UpdateBackstageShortcut()
{
	AStageBackstageShortcut* Closest = nullptr;
	if (IsLocallyControlled() && IsPlayerControlled() && GetCharacterMovement()->IsMovingOnGround() && !IsPhysicsMontageActive())
	{
		TArray<AActor*> Candidates;
		GetCapsuleComponent()->GetOverlappingActors(Candidates, AStageBackstageShortcut::StaticClass());
		double ClosestDistance = TNumericLimits<double>::Max();
		for (AActor* Candidate : Candidates)
		{
			auto* Shortcut = Cast<AStageBackstageShortcut>(Candidate);
			if (!IsValid(Shortcut) || !Shortcut->GetPartner()) continue;
			const double Distance = FVector::DistSquared2D(GetActorLocation(), Shortcut->GetActorLocation());
			if (Distance >= ClosestDistance) continue;
			Closest = Shortcut;
			ClosestDistance = Distance;
		}
	}
	SetNearbyShortcut(Closest);
}

void APlayerCharacter::SetNearbyShortcut(AStageBackstageShortcut* Shortcut)
{
	if (NearbyShortcut.Get() == Shortcut) return;
	if (auto* Previous = NearbyShortcut.Get()) Previous->SetNearby(false);
	NearbyShortcut = Shortcut;
	if (Shortcut) Shortcut->SetNearby(true);
}

void APlayerCharacter::CheckDiveForBackstageShortcut()
{
	if (!IsLocallyControlled() || !IsPlayerControlled() || !PhysicalMontageComponent->IsPlayingPhysicsMontage(DiveMontageName))
	{
		PreviousDiveHeadLocation.Reset();
		bTraversedShortcutDuringDive = false;
		return;
	}
	if (bTraversedShortcutDuringDive) return;

	const FVector Head = GetMesh()->GetBoneLocation(DiveHeadBone);
	const FVector Previous = PreviousDiveHeadLocation.Get(Head);
	PreviousDiveHeadLocation = Head;
	const bool bSimulating = PhysicalMontageComponent->IsSimulatingPhysicsMontage();
	if ((!bSimulating && !GetCharacterMovement()->IsFalling()) || Previous.Equals(Head, 0.1)) return;
	const FVector Forward = GetActorForwardVector();

	// The head continues travelling after the dive switches from capsule movement to ragdoll.
	for (TActorIterator<AStageBackstageShortcut> It(GetWorld()); It; ++It)
	{
		const AStageBackstageShortcut* Shortcut = *It;
		if (FVector::DotProduct(Forward, Shortcut->GetActorForwardVector()) >= -0.15 || !Shortcut->IsDiveEntering(Previous, Head)) continue;
		if (const auto* Destination = Shortcut->GetPartner())
			bTraversedShortcutDuringDive = TraverseBackstageShortcut(*Shortcut, *Destination);
		break;
	}
}

bool APlayerCharacter::TraverseBackstageShortcut(const AStageBackstageShortcut& Source, const AStageBackstageShortcut& Destination)
{
	const FTransform Entrance = Source.GetEntranceTransform();
	const FTransform Exit = Destination.GetEntranceTransform();
	// Both openings face out of the cloth: a half-turn maps travel into one to travel out of the other.
	const FQuat Rotation = Exit.GetRotation() * FQuat(FVector::UpVector, UE_DOUBLE_PI) * Entrance.GetRotation().Inverse();
	const FTransform Link(Rotation, Exit.GetLocation() - Rotation.RotateVector(Entrance.GetLocation()));
	return PhysicalMontageComponent->RedirectPhysicsMontage(Link, Destination.GetExitFloorLocation(),
	    Destination.GetActorForwardVector().GetSafeNormal2D(), GetHeldPhysicsComponent());
}

void APlayerCharacter::OnSprintInput(const FInputActionInstance& Instance)
{
	if (Instance.GetTriggerEvent() == ETriggerEvent::Started)
	{
		TargetSpeed = SprintSpeed;
	}
	else if (Instance.GetTriggerEvent() == ETriggerEvent::Completed || Instance.GetTriggerEvent() == ETriggerEvent::Canceled)
	{
		TargetSpeed = WalkSpeed;
	}
}

const UPrimitiveComponent* APlayerCharacter::GetHeldPhysicsComponent() const
{
	return PhysicsManipulationComponent->GetManipulatedComponent();
}
