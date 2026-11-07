// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/Physics/PhysicalMontageComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TimerManager.h"

namespace
{
	const FName PelvisBone(TEXT("pelvis"));
	const FName RagdollProfile(TEXT("Ragdoll"));

	bool FindRecoveryLocation(const UCapsuleComponent& Capsule, const UCharacterMovementComponent& Movement, const FVector& GroundProbe, FVector& Location)
	{
		const float HalfHeight = Capsule.GetScaledCapsuleHalfHeight();
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule.GetScaledCapsuleRadius(), HalfHeight);
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(PhysicalMontageRecovery), false, Capsule.GetOwner());
		const FCollisionResponseParams Responses(Capsule.GetCollisionResponseToChannels());
		const FVector Start = GroundProbe + FVector::UpVector * (HalfHeight + 20.0f);
		const FVector End = GroundProbe + FVector::UpVector * (HalfHeight - 70.0f);

		FHitResult Hit;
		if (Capsule.GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
			Capsule.GetCollisionObjectType(), Shape, Query, Responses) && !Hit.bStartPenetrating && Movement.IsWalkable(Hit))
		{
			Location = Hit.Location + FVector::UpVector * 2.0f;
			return true;
		}
		return false;
	}
}

UPhysicalMontageComponent::UPhysicalMontageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPhysicalMontageComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh) return;

	SetSkeletalMeshComponent(Mesh);
	ApplyPhysicalAnimationSettingsBelow(PelvisBone, PhysicalAnimationData, false);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBone, BlendWeight);
}

void UPhysicalMontageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActivationTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UPhysicalMontageComponent::StartPhysicsMontage(FName MontageName)
{
	if (!Cast<ACharacter>(GetOwner()) || IsPhysicsMontageActive() || !PreparePhysics(MontageName)) return;

	if (PhysicsActivationDelay > 0.0f)
	{
		FTimerDelegate Delegate = Delegate.CreateUObject(this, &ThisClass::ActivatePhysics, MontageName);
		GetWorld()->GetTimerManager().SetTimer(ActivationTimerHandle, Delegate, PhysicsActivationDelay, false);
	}
	else
	{
		ActivatePhysics(MontageName);
	}
}

bool UPhysicalMontageComponent::PreparePhysics(FName MontageName)
{
	if (MontageName.IsNone()) return false;
	
	UAnimMontage** AnimMontage = AnimMontages.Find(MontageName);
	
	USkeletalMeshComponent* Mesh = GetSkeletalMesh();
	const UPhysicsAsset* Asset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	if (!Asset || Asset->FindBodyIndex(PelvisBone) == INDEX_NONE) return false;

	CurrentCollisionProfile = Mesh->GetCollisionProfileName();
	CurrentCollisionEnabled = Mesh->GetCollisionEnabled();
	CurrentCollisionObjectType = Mesh->GetCollisionObjectType();
	CurrentCollisionResponses = Mesh->GetCollisionResponseToChannels();
	bMeshCollisionSaved = true;

	// During the animated part of the jump the mesh hits props and the capsule handles the floor.
	Mesh->SetCollisionProfileName(RagdollProfile);
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);

	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance(); AnimInstance && AnimMontage)
	{
		AnimInstance->Montage_Play(*AnimMontage);
	}
	return true;
}

void UPhysicalMontageComponent::ActivatePhysics(FName MontageName)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = GetSkeletalMesh();
	if (!Character || !Mesh || bIsSimActive) return;
	if (!bMeshCollisionSaved && !PreparePhysics(MontageName)) return;

	GetWorld()->GetTimerManager().ClearTimer(ActivationTimerHandle);
	Mesh->SetCollisionProfileName(RagdollProfile);
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	const FBodyInstance* Pelvis = Mesh->GetBodyInstance(PelvisBone);
	if (!Pelvis || !Pelvis->IsValidBodyInstance())
	{
		RestoreMeshCollision();
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	// A zero delay can reach this function before the movement tick consumes LaunchCharacter.
	Movement->HandlePendingLaunch();
	const FVector InitialVelocity = Movement->Velocity;
	MeshRelativeTransform = Mesh->GetRelativeTransform();
	SavedStrength = StrengthMultiplyer;
	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	SetStrengthMultiplyer(0.0f);
	Mesh->SetAllBodiesBelowSimulatePhysics(PelvisBone, true, true);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBone, 1.0f);
	Mesh->SetAllPhysicsLinearVelocity(InitialVelocity);
	bIsSimActive = true;

	GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this,
		&ThisClass::RestorePhysics, TimeToRecovery, false);
}

void UPhysicalMontageComponent::RestorePhysics()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = GetSkeletalMesh();
	if (!Character || !Mesh || !bIsSimActive) return;

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FBodyInstance* Pelvis = Mesh->GetBodyInstance(PelvisBone);
	const FVector PelvisLocation = Pelvis ? Pelvis->GetUnrealWorldTransform().GetLocation() : Mesh->GetBoneLocation(PelvisBone);
	FVector RecoveryLocation = Capsule->GetComponentLocation();

	bool bFoundFloor = FindRecoveryLocation(*Capsule, *Movement, PelvisLocation, RecoveryLocation);
	if (!bFoundFloor)
	{
		// A recovery timer can expire during a long fall. Wait for ground before standing up.
		const float KillZ = GetWorld()->GetWorldSettings()->KillZ;
		if (PelvisLocation.Z > KillZ && Mesh->GetPhysicsLinearVelocity(PelvisBone).Z < -80.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this, &ThisClass::RestorePhysics, 0.15f, false);
			return;
		}

		const FVector CapsuleFeet = Capsule->GetComponentLocation() - FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight();
		bFoundFloor = FindRecoveryLocation(*Capsule, *Movement, CapsuleFeet, RecoveryLocation);
	}

	GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	Mesh->SetAllBodiesBelowSimulatePhysics(PelvisBone, false, true);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBone, BlendWeight);
	RestoreMeshCollision();
	SetStrengthMultiplyer(SavedStrength);
	Capsule->SetWorldLocation(RecoveryLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetRelativeTransform(MeshRelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);

	bIsSimActive = false;
	Movement->SetMovementMode(bFoundFloor ? MOVE_Walking : MOVE_Falling);
	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance(); AnimInstance && RecoveryMontage)
	{
		AnimInstance->Montage_Play(RecoveryMontage);
	}
}

void UPhysicalMontageComponent::RestoreMeshCollision()
{
	USkeletalMeshComponent* Mesh = GetSkeletalMesh();
	Mesh->SetCollisionProfileName(CurrentCollisionProfile);
	Mesh->SetCollisionEnabled(CurrentCollisionEnabled);
	Mesh->SetCollisionObjectType(CurrentCollisionObjectType);
	Mesh->SetCollisionResponseToChannels(CurrentCollisionResponses);
	bMeshCollisionSaved = false;
}
