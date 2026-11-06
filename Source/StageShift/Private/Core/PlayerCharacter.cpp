// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Core/PlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "StageShift/Public/Components/Interaction/InteractionComponent.h"
#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"


APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>("Interaction Component");
	PhysicsManipulationComponent = CreateDefaultSubobject<UPhysicsManipulationComponent>("Physics Manipulation Component");
	CarryPoint = CreateDefaultSubobject<USceneComponent>("Carry Point");
	CarryPoint->SetupAttachment(GetRootComponent());
	CarryPoint->SetRelativeLocation(FVector(75.0, 0.0, 25.0));
	PhysicsManipulationComponent->SetCarryPoint(CarryPoint);
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
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
	Super::UnPossessed();
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
	
	if (InteractInputAction)
	{
		Input->BindAction(InteractInputAction, ETriggerEvent::Started, this, &ThisClass::OnInteractionInput);
		Input->BindAction(InteractInputAction, ETriggerEvent::Completed, this, &ThisClass::OnInteractionInput);
		Input->BindAction(InteractInputAction, ETriggerEvent::Canceled, this, &ThisClass::OnInteractionInput);
	}
	
	if (JumpInputAction)
	{
		Input->BindAction(JumpInputAction, ETriggerEvent::Started, this, &ThisClass::OnJumpInput);
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

void APlayerCharacter::OnInteractionInput(const FInputActionInstance& Instance)
{
	switch (Instance.GetTriggerEvent())
	{
	case ETriggerEvent::Started:
		{
			FInteractionPayload Payload;
			InteractionComponent->BeginInteraction(FInteractionContext(), Payload);
			break;
		}
	case ETriggerEvent::Canceled:
		{
			InteractionComponent->CancelInteraction();
			break;
		}
	case ETriggerEvent::Completed:
		{
			InteractionComponent->EndInteraction();
			break;
		}
	default: return;
	}
}

void APlayerCharacter::OnJumpInput(const FInputActionInstance& Instance)
{
}

