// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Core/PlayerCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/CollisionProfile.h"
#include "StageShift/Public/Components/Interaction/InteractionComponent.h"
#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"


APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	ConfigureMovementCollision();
	GetCharacterMovement()->InitialPushForceFactor = 75.0f;
	GetCharacterMovement()->PushForceFactor = 3000.0f;
	GetCharacterMovement()->StandingDownwardForceScale = 0.0f;
	
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>("Interaction Component");
	PhysicsManipulationComponent = CreateDefaultSubobject<UPhysicsManipulationComponent>("Physics Manipulation Component");
	PhysicalMontageComponent = CreateDefaultSubobject<UPhysicalMontageComponent>("Physical Montage Component");
	CarryPoint = CreateDefaultSubobject<USceneComponent>("Carry Point");
	CarryPoint->SetupAttachment(GetRootComponent());
	CarryPoint->SetRelativeLocation(FVector(75.0, 0.0, 25.0));
	PhysicsManipulationComponent->SetCarryPoint(CarryPoint);
}

void APlayerCharacter::ConfigureMovementCollision()
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);

	USkeletalMeshComponent* MeshComponent = GetMesh();
	MeshComponent->SetCollisionProfileName(TEXT("CharacterMesh"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);

	GetCharacterMovement()->bEnablePhysicsInteraction = true;
}

void APlayerCharacter::BeginPlay()
{
	ConfigureMovementCollision();
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
	Super::UnPossessed();
}

void APlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bStumblePending)
	{
		bStumblePending = false;
		PhysicalMontageComponent->StartPhysicsMontage(TEXT("Fall"), StumbleVelocity);
	}
	
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
	if (Instance.GetTriggerEvent() == ETriggerEvent::Completed || Instance.GetTriggerEvent() == ETriggerEvent::Canceled)
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
		bUseControllerRotationYaw = false;
		return;
	}
	
	if (!InteractionComponent->HasActiveInteraction()) return;
	
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->PlayerCameraManager) return;

	const FVector2D Axis = Instance.GetValue().Get<FVector2D>();
	
    GetCharacterMovement()->bOrientRotationToMovement = false;

	const float CameraYaw = PC->PlayerCameraManager->GetCameraRotation().Yaw;

	const FRotationMatrix Basis(FRotator(0.f, CameraYaw, 0.f));

	const FVector Direction = Basis.GetUnitAxis(EAxis::X) * Axis.Y * -1.0f + Basis.GetUnitAxis(EAxis::Y) * Axis.X;

	SetActorRotation(Direction.Rotation());
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

void APlayerCharacter::MoveBlockedBy(const FHitResult& Impact)
{
	Super::MoveBlockedBy(Impact);

	if (bStumblePending || PhysicalMontageComponent->IsPhysicsMontageActive() ||
		!GetCharacterMovement()->IsMovingOnGround() || !Impact.IsValidBlockingHit()) return;

	const UPrimitiveComponent* Obstacle = Impact.GetComponent();
	if (!Obstacle || Obstacle == PhysicsManipulationComponent->GetManipulatedComponent() ||
		!Obstacle->IsSimulatingPhysics(Impact.BoneName)) return;

	const FVector Velocity = GetVelocity();
	if (Velocity.SizeSquared2D() < FMath::Square(150.0)) return;

	const FVector ToHit = (Impact.ImpactPoint - GetActorLocation()).GetSafeNormal2D();
	if (FVector::DotProduct(Velocity.GetSafeNormal2D(), ToHit) < 0.45) return;

	const float KneeZ = FMath::Min(GetMesh()->GetSocketLocation(TEXT("calf_l")).Z, GetMesh()->GetSocketLocation(TEXT("calf_r")).Z);
	if (Obstacle->Bounds.GetBox().Max.Z > KneeZ - 5.0f || Impact.ImpactPoint.Z > KneeZ) return;
	
	StumbleVelocity = FVector(Velocity.X, Velocity.Y, 0.0);
	bStumblePending = true;
}

