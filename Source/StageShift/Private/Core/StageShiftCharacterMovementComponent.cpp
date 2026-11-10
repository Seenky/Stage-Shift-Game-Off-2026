#include "Core/StageShiftCharacterMovementComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/Physics/DollPhysicsComponent.h"
#include "Core/StageShiftCharacter.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

bool UStageShiftCharacterMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{
	if (const auto* Character = Cast<AStageShiftCharacter>(CharacterOwner); Character && Character->IsDollPhysicsActive())
	{
		if (Teleport != ETeleportType::None) Character->GetDollPhysics()->ResetAfterTeleport();
		const FVector LimitedDelta = bSweep && Teleport == ETeleportType::None ? Character->GetDollPhysics()->ConstrainMovement(Delta) : Delta;
		
		return Super::MoveUpdatedComponentImpl(LimitedDelta, NewRotation, bSweep, OutHit, Teleport);
	}
	FHitResult BodyHit;
	if (!bSweep || Teleport != ETeleportType::None || !IsMovingOnGround() || Delta.SizeSquared2D() < UE_KINDA_SMALL_NUMBER ||
		!SweepCharacterBody(Delta, BodyHit))
	{
		return Super::MoveUpdatedComponentImpl(Delta, NewRotation, bSweep, OutHit, Teleport);
	}
	
	const double Limit = FMath::Max(BodyHit.Time - 0.35 / Delta.Size(), 0.0);
	const FVector Start = UpdatedComponent->GetComponentLocation();
	FHitResult CapsuleHit;
	const bool bMoved = Super::MoveUpdatedComponentImpl(Delta * Limit, NewRotation, true, &CapsuleHit, Teleport);
	if (OutHit)
	{
		if (CapsuleHit.bBlockingHit)
		{
			*OutHit = CapsuleHit;
			OutHit->Time *= Limit;
		}
		else
		{
			*OutHit = BodyHit;
			OutHit->Time = Limit;
			OutHit->Location = UpdatedComponent->GetComponentLocation();
		}
		OutHit->TraceStart = Start;
		OutHit->TraceEnd = Start + Delta;
	}
	return bMoved;
}

bool UStageShiftCharacterMovementComponent::SweepCharacterBody(const FVector& Delta, FHitResult& OutHit) const
{
	const auto* Character = Cast<AStageShiftCharacter>(CharacterOwner);
	if (!Character || Character->IsPhysicsMontageActive()) return false;

	const USkeletalMeshComponent* Body = Character->GetMesh();
	const UPhysicsAsset* PhysicsAsset = Body->GetPhysicsAsset();
	const UWorld* World = GetWorld();
	if (!PhysicsAsset || !World) return false;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(StageShiftBodyContact), false, Character);
	Query.AddIgnoredComponent(Character->GetHeldPhysicsComponent());
	const FCollisionObjectQueryParams Objects(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic) | ECC_TO_BITFIELD(ECC_PhysicsBody));
	const FVector MoveDirection = Delta.GetSafeNormal2D();
	TArray<FHitResult> Hits;
	OutHit.Time = 1.0f;

	const auto SweepShape = [&](const FVector& Center, const FQuat& Rotation, const FCollisionShape& Shape, FName Bone)
	{
		if (Shape.IsNearlyZero()) return;
		Hits.Reset();
		World->SweepMultiByObjectType(Hits, Center, Center + Delta * OutHit.Time, Rotation, Objects, Shape, Query);
		const float TestedFraction = OutHit.Time;
		for (const FHitResult& Hit : Hits)
		{
			const UPrimitiveComponent* Obstacle = Hit.GetComponent();
			if (!Obstacle || (Obstacle->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block &&
				!Obstacle->IsSimulatingPhysics(Hit.BoneName))) continue;

			const FVector Normal = Hit.ImpactNormal;
			if (FMath::Abs(Normal.Z) >= 0.65 || FVector::DotProduct(MoveDirection, Normal) >= -0.1) continue;
			const float HitTime = Hit.Time * TestedFraction;
			if (OutHit.bBlockingHit && HitTime >= OutHit.Time) continue;

			OutHit = Hit;
			OutHit.Time = HitTime;
			OutHit.MyBoneName = Bone;
			OutHit.bBlockingHit = true;
			OutHit.bStartPenetrating = false;
			OutHit.Normal = Normal;
		}
	};

	for (const USkeletalBodySetup* Setup : PhysicsAsset->SkeletalBodySetups)
	{
		if (!Setup || Body->GetBoneIndex(Setup->BoneName) == INDEX_NONE) continue;

		const FTransform Bone = Body->GetSocketTransform(Setup->BoneName);
		const FVector Scale = Bone.GetScale3D().GetAbs();
		for (const FKSphereElem& Sphere : Setup->AggGeom.SphereElems)
		{
			SweepShape(Bone.TransformPosition(Sphere.Center), FQuat::Identity,
				FCollisionShape::MakeSphere(Sphere.Radius * Scale.GetMin()), Setup->BoneName);
		}
		for (const FKSphylElem& Capsule : Setup->AggGeom.SphylElems)
		{
			const double Radius = Capsule.Radius * FMath::Min(Scale.X, Scale.Y);
			SweepShape(Bone.TransformPosition(Capsule.Center), Bone.TransformRotation(Capsule.Rotation.Quaternion()),
				FCollisionShape::MakeCapsule(Radius, Capsule.Length * Scale.Z * 0.5 + Radius), Setup->BoneName);
		}
		for (const FKBoxElem& Box : Setup->AggGeom.BoxElems)
		{
			const FVector Extent = FVector(Box.X, Box.Y, Box.Z) * Scale * 0.5;
			SweepShape(Bone.TransformPosition(Box.Center), Bone.TransformRotation(Box.Rotation.Quaternion()),
				FCollisionShape::MakeBox(Extent), Setup->BoneName);
		}
	}
	return OutHit.bBlockingHit;
}
