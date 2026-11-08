// Fill out your copyright notice in the Description page of Project Settings.

#include "Components/Physics/PhysicalMontageComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/StageShiftAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TimerManager.h"

namespace
{
	const FName PelvisBone(TEXT("pelvis"));
	const FName RagdollProfile(TEXT("Ragdoll"));

	bool FindRecoveryLocation(const UCapsuleComponent& Capsule, const UCharacterMovementComponent& Movement, ECollisionResponse PhysicsBodyResponse, const FVector& GroundProbe, FVector& Location)
	{
		const float HalfHeight = Capsule.GetScaledCapsuleHalfHeight();
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule.GetScaledCapsuleRadius(), HalfHeight);
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(PhysicalMontageRecovery), false, Capsule.GetOwner());
		FCollisionResponseParams Responses(Capsule.GetCollisionResponseToChannels());
		Responses.CollisionResponse.SetResponse(ECC_PhysicsBody, PhysicsBodyResponse);
		const FVector Start = GroundProbe + FVector::UpVector * (HalfHeight + 20.0f);
		const FVector End = GroundProbe + FVector::UpVector * (HalfHeight - 70.0f);

		FHitResult Hit;
		if (Capsule.GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
			Capsule.GetCollisionObjectType(), Shape, Query, Responses) && !Hit.bStartPenetrating && Movement.IsWalkable(Hit))
		{
			const FVector Candidate = Hit.Location + FVector::UpVector * 2.0f;
			if (!Capsule.GetWorld()->OverlapBlockingTestByChannel(Candidate, FQuat::Identity,
				Capsule.GetCollisionObjectType(), Shape, Query, Responses))
			{
				Location = Candidate;
				return true;
			}
		}
		return false;
	}
}

FPhysicalMontageSettings::FPhysicalMontageSettings()
{
	PhysicalAnimationData.bIsLocalSimulation = true;
	PhysicalAnimationData.OrientationStrength = 2000.0f;
	PhysicalAnimationData.AngularVelocityStrength = 100.0f;
}

UPhysicalMontageComponent::UPhysicalMontageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	MontageSettings.Add(TEXT("Fall"));
	FPhysicalMontageSettings& JumpSettings = MontageSettings.Add(TEXT("Jump"));
	JumpSettings.LaunchDelay = 0.15f;
	JumpSettings.PhysicsActivationDelay = 0.25f;
	JumpSettings.LaunchVelocity = FVector(500.0, 0.0, 125.0);
}

void UPhysicalMontageComponent::PostLoad()
{
	Super::PostLoad();
}

void UPhysicalMontageComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	if (!Mesh) return;
	
	SetSkeletalMeshComponent(Mesh);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBone, 0.0f);
}

void UPhysicalMontageComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LaunchTimerHandle);
		World->GetTimerManager().ClearTimer(ActivationTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UPhysicalMontageComponent::StartPhysicsMontage(FName MontageName)
{
	if (const AActor* Owner = GetOwner())
	{
		StartPhysicsMontage(MontageName, Owner->GetVelocity());
	}
}

void UPhysicalMontageComponent::StartPhysicsMontage(FName MontageName, const FVector& InitialVelocity)
{
	if (IsPhysicsMontageActive() || !PreparePhysics(MontageName, InitialVelocity)) return;

	if (ActiveSettings.LaunchDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(LaunchTimerHandle, this, &ThisClass::LaunchMontage, ActiveSettings.LaunchDelay, false);
	}
	else
	{
		LaunchMontage();
	}

	const float ActivationDelay = FMath::Max(ActiveSettings.LaunchDelay, ActiveSettings.PhysicsActivationDelay);
	if (ActivationDelay > 0.0f)
	{
		FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ThisClass::ActivatePhysics, MontageName);
		GetWorld()->GetTimerManager().SetTimer(ActivationTimerHandle, Delegate, ActivationDelay, false);
	}
	else
	{
		ActivatePhysics(MontageName);
	}
}

bool UPhysicalMontageComponent::PreparePhysics(FName MontageName, const FVector& InitialVelocity)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const FPhysicalMontageSettings* Settings = MontageSettings.Find(MontageName);
	USkeletalMeshComponent* Mesh = GetSkeletalMesh();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const UPhysicsAsset* Asset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	if (!Character || !Settings || !Settings->Montage || !AnimInstance ||
		!Asset || Asset->FindBodyIndex(PelvisBone) == INDEX_NONE) return false;
	if (AnimInstance->Montage_Play(Settings->Montage) <= 0.0f) return false;

	ActiveSettings = *Settings;
	ActiveMontageName = MontageName;
	MontageLaunchVelocity = InitialVelocity * ActiveSettings.InheritedVelocityScale + Character->GetActorRotation().RotateVector(ActiveSettings.LaunchVelocity);
	SavedCapsulePhysicsBodyResponse = Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_PhysicsBody);
	CurrentCollisionProfile = Mesh->GetCollisionProfileName();
	CurrentCollisionEnabled = Mesh->GetCollisionEnabled();
	CurrentCollisionObjectType = Mesh->GetCollisionObjectType();
	CurrentCollisionResponses = Mesh->GetCollisionResponseToChannels();
	bMeshCollisionSaved = true;
	return true;
}

void UPhysicalMontageComponent::LaunchMontage()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !bMeshCollisionSaved || bIsSimActive) return;

	GetWorld()->GetTimerManager().ClearTimer(LaunchTimerHandle);
	
	Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	Character->LaunchCharacter(MontageLaunchVelocity, true, true);
}

void UPhysicalMontageComponent::ActivatePhysics(FName MontageName)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = GetSkeletalMesh();
	if (!Character || !Mesh || bIsSimActive || bIsRecovering) return;
	if (!bMeshCollisionSaved)
	{
		if (!PreparePhysics(MontageName, Character->GetVelocity())) return;
		LaunchMontage();
	}
	if (MontageName != ActiveMontageName) return;
	if (GetWorld()->GetTimerManager().IsTimerActive(LaunchTimerHandle))
	{
		LaunchMontage();
	}

	GetWorld()->GetTimerManager().ClearTimer(ActivationTimerHandle);
	Mesh->HandleExistingParallelEvaluationTask(true, true);
	Mesh->SetCollisionProfileName(RagdollProfile);
	Mesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	const FBodyInstance* Pelvis = Mesh->GetBodyInstance(PelvisBone);
	if (!Pelvis || !Pelvis->IsValidBodyInstance())
	{
		RestoreMeshCollision();
		return;
	}

	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

	Movement->HandlePendingLaunch();
	const FVector InitialVelocity = Movement->Velocity;
	MeshRelativeTransform = Mesh->GetRelativeTransform();
	SavedStrength = StrengthMultiplyer;
	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	ApplyPhysicalAnimationSettingsBelow(PelvisBone, ActiveSettings.PhysicalAnimationData, false);
	
	FPhysicalAnimationData PelvisDrive = ActiveSettings.PhysicalAnimationData;
	PelvisDrive.bIsLocalSimulation = false;
	PelvisDrive.PositionStrength = 0.0f;
	PelvisDrive.VelocityStrength = 0.0f;

	ApplyPhysicalAnimationSettings(PelvisBone, PelvisDrive);
	
	SetStrengthMultiplyer(ActiveSettings.StrengthMultiplier);
	
	Mesh->UpdateKinematicBonesToAnim(Mesh->GetComponentSpaceTransforms(), ETeleportType::TeleportPhysics, false, EAllowKinematicDeferral::DisallowDeferral);
	Mesh->SetAllBodiesBelowSimulatePhysics(PelvisBone, true, true);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBone, ActiveSettings.PhysicsBlendWeight);
	Mesh->SetAllPhysicsLinearVelocity(InitialVelocity);
	bIsSimActive = true;

	GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this, &ThisClass::RestorePhysics, FMath::Max(ActiveSettings.TimeToRecovery, 0.1f), false);
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
	FVector RecoveryLocation;

	bool bFoundFloor = FindRecoveryLocation(*Capsule, *Movement, SavedCapsulePhysicsBodyResponse, PelvisLocation, RecoveryLocation);

	for (int32 Ring = 1; Ring <= 2 && !bFoundFloor; ++Ring)
	{
		const float Radius = Capsule->GetScaledCapsuleRadius() * Ring;
		for (int32 Direction = 0; Direction < 8 && !bFoundFloor; ++Direction)
		{
			const float Angle = FMath::DegreesToRadians(Direction * 45.0f);
			const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);
			bFoundFloor = FindRecoveryLocation(*Capsule, *Movement, SavedCapsulePhysicsBodyResponse,
				PelvisLocation + Offset, RecoveryLocation);
		}
	}
	if (!bFoundFloor)
	{
		GetWorld()->GetTimerManager().SetTimer(RecoveryTimerHandle, this, &ThisClass::RestorePhysics, 0.15f, false);
		return;
	}

	Mesh->HandleExistingParallelEvaluationTask(true, true);
	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	UStageShiftAnimInstance* StageAnimInstance = Cast<UStageShiftAnimInstance>(AnimInstance);
	const FTransform PreviousMeshTransform = Mesh->GetComponentTransform();
	FPoseSnapshot RagdollPose;
	if (StageAnimInstance)
	{
		StageAnimInstance->SnapshotPose(RagdollPose);
	}

	FRotator RecoveryRotation = Capsule->GetComponentRotation();
	if (RecoveryMontage)
	{
		const FVector BodyDirection = Mesh->GetBoneLocation(TEXT("spine_03")) - PelvisLocation;
		if (BodyDirection.SizeSquared2D() > FMath::Square(10.0))
		{
			RecoveryRotation.Yaw = BodyDirection.Rotation().Yaw;
		}
	}

	GetWorld()->GetTimerManager().ClearTimer(RecoveryTimerHandle);
	Mesh->SetAllBodiesBelowSimulatePhysics(PelvisBone, false, true);
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(PelvisBone, 0.0f);
	SetStrengthMultiplyer(SavedStrength);
	Capsule->SetWorldLocationAndRotation(RecoveryLocation, RecoveryRotation, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetRelativeTransform(MeshRelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);
	RestoreMeshCollision();

	bIsSimActive = false;
	const bool bHasPoseBlend = StageAnimInstance && StageAnimInstance->BlendFromRagdollPose(MoveTemp(RagdollPose), PreviousMeshTransform, RecoveryPoseBlendTime);
	if (AnimInstance && RecoveryMontage)
	{
		FAlphaBlendArgs BlendIn = RecoveryMontage->GetBlendInArgs();
		if (bHasPoseBlend)
		{
			BlendIn.BlendTime = 0.0f;
		}
		if (AnimInstance->Montage_PlayWithBlendIn(RecoveryMontage, BlendIn) > 0.0f)
		{
			bIsRecovering = true;
			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &ThisClass::OnRecoveryMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, RecoveryMontage);
		}
	}
	if (!bIsRecovering)
	{
		if (AnimInstance)
		{
			AnimInstance->Montage_Stop(0.0f, ActiveSettings.Montage);
		}
		Movement->SetMovementMode(MOVE_Walking);
	}
	if (bHasPoseBlend)
	{
		// Publish the snapshot with the get-up montage already selected as its target.
		Mesh->TickAnimation(0.0f, false);
		Mesh->RefreshBoneTransforms(nullptr);
		Mesh->RefreshFollowerComponents();
	}
}

void UPhysicalMontageComponent::OnRecoveryMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bIsRecovering) return;

	bIsRecovering = false;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}


void UPhysicalMontageComponent::RestoreMeshCollision()
{
	GetWorld()->GetTimerManager().ClearTimer(LaunchTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(ActivationTimerHandle);
	USkeletalMeshComponent* Mesh = GetSkeletalMesh();
	Mesh->SetCollisionProfileName(CurrentCollisionProfile);
	Mesh->SetCollisionEnabled(CurrentCollisionEnabled);
	Mesh->SetCollisionObjectType(CurrentCollisionObjectType);
	Mesh->SetCollisionResponseToChannels(CurrentCollisionResponses);
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, SavedCapsulePhysicsBodyResponse);
	}
	ActiveMontageName = NAME_None;
	bMeshCollisionSaved = false;
}
