// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Core/PlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StageShift/Public/Components/Interaction/InteractionComponent.h"
#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"
#include "Components/Interaction/ThrowAimComponent.h"
#include "Components/Interaction/InteractionOutlineComponent.h"


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
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	TargetSpeed = WalkSpeed;
	
	PhysicsManipulationComponent->SetCarryPoint(CarryPoint);
	if (!CarrySocketName.IsNone() && GetMesh()->DoesSocketExist(CarrySocketName))
	{
		CarryPoint->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, CarrySocketName);
	}
	
	if (!MappingContext) return;
	
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		check(IsValid(LocalPlayer));

		UEnhancedInputLocalPlayerSubsystem* EnhancedInputLocalPlayerSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		check(IsValid(EnhancedInputLocalPlayerSubsystem));
		
		EnhancedInputLocalPlayerSubsystem->AddMappingContext(MappingContext, 0);
	}
}

void APlayerCharacter::UnPossessed()
{
	InteractionComponent->CancelInteraction();
	ThrowAimComponent->StopAiming();
	Super::UnPossessed();
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!GetCharacterMovement()) return;
	
	GetCharacterMovement()->MaxWalkSpeed = FMath::FInterpTo(GetCharacterMovement()->MaxWalkSpeed, TargetSpeed, DeltaSeconds, MovementBlendSpeed);
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
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager) return;

	const FVector2D Axis = Instance.GetValue().Get<FVector2D>();

	const float CameraYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;

	const FRotationMatrix CameraBasis(FRotator(0.0f, CameraYaw, 0.0f));

	const FVector Forward = CameraBasis.GetUnitAxis(EAxis::X);
	const FVector Right = CameraBasis.GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.X);
	AddMovementInput(Right, Axis.Y);
}

void APlayerCharacter::OnCameraInput(const FInputActionInstance& Instance)
{
	const bool bReleased = Instance.GetTriggerEvent() == ETriggerEvent::Completed || Instance.GetTriggerEvent() == ETriggerEvent::Canceled;
	ThrowAimComponent->SetStickInput(bReleased ? FVector2D::ZeroVector : Instance.GetValue().Get<FVector2D>());
}

void APlayerCharacter::OnJumpInput(const FInputActionInstance& Instance)
{
	if (!GetCharacterMovement()->IsMovingOnGround() || PhysicalMontageComponent->IsPhysicsMontageActive()) return;
	PhysicalMontageComponent->StartPhysicsMontage(TEXT("Jump"));
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
