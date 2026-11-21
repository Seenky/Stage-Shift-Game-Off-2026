// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Core/PlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
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
#include "Stage/Environment/StageSoundControl.h"
#include "UObject/ConstructorHelpers.h"

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
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()); PlayerController && PlayerController->IsLocalController())
	{
		PlayerController->ClearAudioListenerAttenuationOverride();
	}
	InteractionComponent->CancelInteraction();
	ThrowAimComponent->StopAiming();
	Super::UnPossessed();
	OnInteractionCandidateChanged(nullptr);
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GetCharacterMovement()) return;

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
	if (Console == FocusedSoundControl.Get()) return;
	if (auto* Previous = FocusedSoundControl.Get()) Previous->SetSelectorFocused(false);
	FocusedSoundControl = Console;
	bSoundStickHeld = false;
	if (Console) Console->SetSelectorFocused(true);
}

void APlayerCharacter::OnJumpInput(const FInputActionInstance& Instance)
{
	if (PhysicsManipulationComponent->IsDragging() || !GetCharacterMovement()->IsMovingOnGround() || PhysicalMontageComponent->IsPhysicsMontageActive()) return;
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const UEnhancedPlayerInput* Input = PC ? Cast<UEnhancedPlayerInput>(PC->PlayerInput) : nullptr;
	const FVector Direction = Input && MoveInputAction ? MovementInputToWorld(Input->GetActionValue(MoveInputAction)).GetSafeNormal2D() : FVector::ZeroVector;
	PhysicalMontageComponent->StartPhysicsMontage(TEXT("Jump"), GetVelocity(), Direction);
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
