#include "Animation/StageShiftAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Core/StageShiftCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KismetAnimationLibrary.h"
#include "Physics/PhysicsManipulationComponent.h"

namespace
{
	const FName SpeedDataCurve(TEXT("Speed_Data"));
	const FName PlayRateCurve(TEXT("PlayRate"));
	const FName GaitCurve(TEXT("Gait"));
	const FName LeftFootPlantCurve(TEXT("FootPlant_L"));
	const FName RightFootPlantCurve(TEXT("FootPlant_R"));

	constexpr float WalkStride = 0.5f;
	constexpr float MinimumMovingSpeed = 1.0f;
	constexpr float FootIKResponse = 12.0f;
}

void UStageShiftAnimInstance::NativeInitializeAnimation()
{
	PickupAlpha = 0.0f;
	Super::NativeInitializeAnimation();

	GroundSpeed = 0.0f;
	Direction = 0.0f;
	Stride = 0.0f;
	Gait = 0.0f;
	PlayRate = 1.0f;
	FootIKAlpha = 0.0f;
	RecoveryPose.Reset();
	RecoveryPoseAlpha = 0.0f;
	RecoveryBlendTime = 0.0f;
	RecoveryBlendElapsed = 0.0f;
	DollMotion = {};
	DragPose = {};
}

bool UStageShiftAnimInstance::BlendFromRagdollPose(FPoseSnapshot&& Pose, const FTransform& PreviousMeshTransform, float BlendTime)
{
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	const USkeletalMesh* SkeletalMesh = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!SkeletalMesh || !Pose.bIsValid || Pose.LocalTransforms.IsEmpty() ||
		Pose.LocalTransforms.Num() != Pose.BoneNames.Num() || BlendTime <= 0.0f)
	{
		RecoveryPoseAlpha = 0.0f;
		return false;
	}
	
	const FTransform OldToNewMesh = PreviousMeshTransform.GetRelativeTransform(Mesh->GetComponentTransform());
	const FReferenceSkeleton& Skeleton = SkeletalMesh->GetRefSkeleton();
	const FTransform& ReferenceRoot = Skeleton.GetRefBonePose()[0];
	const FTransform RootOffset = (Pose.LocalTransforms[0] * OldToNewMesh).GetRelativeTransform(ReferenceRoot);
	
	for (int32 PoseIndex = 1; PoseIndex < Pose.LocalTransforms.Num(); ++PoseIndex)
	{
		const int32 BoneIndex = Skeleton.FindBoneIndex(Pose.BoneNames[PoseIndex]);
		if (BoneIndex != INDEX_NONE && Skeleton.GetParentIndex(BoneIndex) == 0)
		{
			Pose.LocalTransforms[PoseIndex] *= RootOffset;
		}
	}
	Pose.LocalTransforms[0] = ReferenceRoot;
	RecoveryPose = MoveTemp(Pose);
	RecoveryPoseAlpha = 1.0f;
	RecoveryBlendTime = BlendTime;
	RecoveryBlendElapsed = 0.0f;
	return true;
}

void UStageShiftAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (RecoveryPoseAlpha > 0.0f)
	{
		RecoveryBlendElapsed += DeltaSeconds;
		RecoveryPoseAlpha = 1.0f - FMath::SmoothStep(0.0f, RecoveryBlendTime, RecoveryBlendElapsed);
	}

	const APawn* Pawn = TryGetPawnOwner();
	const AStageShiftCharacter* StageCharacter = Cast<AStageShiftCharacter>(Pawn);
	bDollPhysicsActive = StageCharacter && StageCharacter->IsDollPhysicsActive();
	float TargetPickupAlpha = 0.0f;
	const UPhysicsManipulationComponent* Manipulation = Pawn ? Pawn->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	const bool bDragging = Manipulation && Manipulation->GetDragPose(DragPose.Hands[0], DragPose.Hands[1], DragPose.Facing, DragPose.Effort) &&
		(!StageCharacter || !StageCharacter->IsPhysicsMontageActive());
	DragPose.Alpha = FMath::FInterpTo(DragPose.Alpha, bDragging ? 1.0f : 0.0f, DeltaSeconds, bDragging ? 10.0f : 12.0f);
	if (StageCharacter && StageCharacter->IsPhysicsMontageActive()) DragPose.Alpha = 0.0f;
	if (DragPose.Alpha < KINDA_SMALL_NUMBER) DragPose.Alpha = 0.0f;
	const ACharacter* DragCharacter = Cast<ACharacter>(Pawn);
	const FVector Intent = DragCharacter ? DragCharacter->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D() : FVector::ZeroVector;
	const FVector TargetDragLean = bDragging ? DragPose.Facing * FVector::DotProduct(Intent, DragPose.Facing) * (0.04f + 0.08f * DragPose.Effort) : FVector::ZeroVector;
	DragPose.Lean = FMath::VInterpTo(DragPose.Lean, TargetDragLean, DeltaSeconds, 8.0f);
	if (Manipulation)
	{
		Manipulation->GetPickupTarget(PickupTarget, TargetPickupAlpha, &PickupHandOffset);
	}
	// Reach follows the contact curve exactly; release eases out even after the montage ends.
	PickupAlpha = TargetPickupAlpha >= PickupAlpha ? TargetPickupAlpha
		: FMath::FInterpTo(PickupAlpha, TargetPickupAlpha, DeltaSeconds, 10.0f);
	if (!Manipulation || !Manipulation->GetManipulatedComponent() ||
		(StageCharacter && StageCharacter->IsPhysicsMontageActive())) PickupAlpha = 0.0f;
	if (PickupAlpha < KINDA_SMALL_NUMBER) PickupAlpha = 0.0f;
	const FVector Velocity = Pawn ? Pawn->GetVelocity() : FVector::ZeroVector;
	GroundSpeed = Velocity.Size2D();
	
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	const FVector PlanarVelocity(Velocity.X, Velocity.Y, 0.0);
	const float MeshSpaceSpeed = Mesh
		? Mesh->GetComponentTransform().InverseTransformVector(PlanarVelocity).Size()
		: GroundSpeed;

	Direction = 0.0f;
	Stride = 0.0f;
	if (Pawn && GroundSpeed >= MinimumMovingSpeed)
	{
		const FRotator Facing(0.0, Pawn->GetActorRotation().Yaw, 0.0);
		Direction = UKismetAnimationLibrary::CalculateDirection(PlanarVelocity, Facing);

		const float AbsDirection = FMath::Abs(Direction);
		const bool bFront = AbsDirection <= 90.0f;
		const FStageShiftGaitSpeeds& From = bFront ? ForwardSpeeds : SideSpeeds;
		const FStageShiftGaitSpeeds& To = bFront ? SideSpeeds : BackwardSpeeds;
		const float DirectionAlpha = (bFront ? AbsDirection : AbsDirection - 90.0f) / 90.0f;
		const float WalkSpeed = FMath::Max(FMath::Lerp(From.Walk, To.Walk, DirectionAlpha), MinimumMovingSpeed);
		const float RunSpeed = FMath::Max(FMath::Lerp(From.Run, To.Run, DirectionAlpha), WalkSpeed + 1.0f);

		Stride = MeshSpaceSpeed <= WalkSpeed
			? WalkStride * MeshSpaceSpeed / WalkSpeed
			: FMath::GetMappedRangeValueClamped(FVector2f(WalkSpeed, RunSpeed), FVector2f(WalkStride, 1.0f), MeshSpaceSpeed);
	}
	
	GetCurveValueWithDefault(GaitCurve, Stride * 2.0f, Gait);
	float SourceRate = 1.0f;
	GetCurveValueWithDefault(PlayRateCurve, 1.0f, SourceRate);
	const float ReferenceSpeed = GetCurveValue(SpeedDataCurve) * FMath::Abs(SourceRate);
	const float TargetPlayRate = GroundSpeed >= MinimumMovingSpeed && ReferenceSpeed >= MinimumMovingSpeed
		? FMath::Clamp(MeshSpaceSpeed / ReferenceSpeed, MinPlayRate, FMath::Max(MinPlayRate, MaxPlayRate))
		: 1.0f;
	PlayRate = FMath::FInterpTo(PlayRate, TargetPlayRate, DeltaSeconds, PlayRateResponse);

	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const bool bGrounded = Character && Character->GetCharacterMovement()->IsMovingOnGround();
	FootIKAlpha = FMath::FInterpTo(FootIKAlpha, bEnableFootIK && bGrounded && RecoveryPoseAlpha <= 0.0f ? 1.0f : 0.0f, DeltaSeconds, FootIKResponse);
	UpdateDollMotion(Cast<AStageShiftCharacter>(Pawn), PlanarVelocity, bGrounded, DeltaSeconds);
}

void UStageShiftAnimInstance::AddLandingSquash(float Strength)
{
	DollMotion.CompressionVelocity += Strength * 60.0f;
}

void UStageShiftAnimInstance::UpdateContactSquash(const FVector& Away, const FVector& ImpactPoint, float ImpactSpeed, float Push,
	bool bNewContact, FName HitBodyBone)
{
	const USkeletalMeshComponent* Mesh = GetOwningComponent();
	const float Impact = bNewContact
		? FMath::GetMappedRangeValueClamped(FVector2f(25.0f, 350.0f), FVector2f(0.0f, 1.0f), ImpactSpeed)
		: 0.0f;
	if (!Mesh || ContactSquashScale <= 0.0f || (Impact <= 0.0f && Push <= 0.0f)) return;

	if (bNewContact || DollMotion.Pose.ContactBone.IsNone() || (!HitBodyBone.IsNone() && HitBodyBone != DollMotion.Pose.ContactBone) ||
		FVector::DistSquared(DollMotion.LastProjectedImpact, ImpactPoint) > FMath::Square(5.0))
	{
		// The capsule reports the hit, but the physics asset gives a point on the posed character.
		FClosestPointOnPhysicsAsset Surface;
		const bool bHasSurface = HitBodyBone.IsNone() && Mesh->GetClosestPointOnPhysicsAsset(ImpactPoint, Surface, false)
			&& !Surface.BoneName.IsNone() && Surface.Distance >= 0.0f;
		const FName AnchorBone = !HitBodyBone.IsNone() ? HitBodyBone : (bHasSurface ? Surface.BoneName : FName(TEXT("spine_03")));
		const FTransform Anchor = Mesh->GetSocketTransform(AnchorBone);
		const FVector AnchorPoint = !HitBodyBone.IsNone() ? ImpactPoint
			: (bHasSurface ? Surface.ClosestWorldPosition : Anchor.GetLocation() - Away * 14.0);
		DollMotion.Pose.ContactAnchor = Anchor.InverseTransformPosition(AnchorPoint);
		DollMotion.Pose.ContactBone = AnchorBone;
		DollMotion.LastProjectedImpact = ImpactPoint;
	}
	DollMotion.Pose.ContactNormal = Away;
	DollMotion.ContactVelocity = FMath::Max(DollMotion.ContactVelocity, 195.0f * Impact * ContactSquashScale);
	DollMotion.ContactHold = 1.5f * Push * ContactSquashScale;
	DollMotion.ContactHoldTime = 0.10f;
}

void UStageShiftAnimInstance::UpdateDollMotion(const AStageShiftCharacter* Character, const FVector& PlanarVelocity, bool bGrounded, float DeltaSeconds)
{
	if (!Character || RecoveryPoseAlpha > 0.0f || Character->IsPhysicsMontageActive())
	{
		DollMotion = {};
		return;
	}

	FStageShiftDollPose& Pose = DollMotion.Pose;
	const FVector2D Contact(GetCurveValue(LeftFootPlantCurve), GetCurveValue(RightFootPlantCurve));
	const double Yaw = Character->GetActorRotation().Yaw;
	FVector TargetLean = FVector::ZeroVector;
	if (bGrounded && DollMotion.bWasGrounded)
	{
		const bool bNewFootPlant = (Contact.X >= 0.8 && DollMotion.PreviousFootContact.X < 0.8)
			|| (Contact.Y >= 0.8 && DollMotion.PreviousFootContact.Y < 0.8);
		if (bNewFootPlant && GroundSpeed > 20.0f)
		{
			DollMotion.CompressionVelocity += Character->GetStepSquash() * 60.0f * FMath::Clamp(GroundSpeed / 150.0f, 0.6f, 1.5f);
		}
		if (DeltaSeconds > SMALL_NUMBER)
		{
			const FVector Acceleration = (PlanarVelocity - DollMotion.PreviousVelocity) / DeltaSeconds;
			const double YawRate = FMath::FindDeltaAngleDegrees(DollMotion.PreviousYaw, Yaw) / DeltaSeconds;
			const FVector TurnLag = Character->GetActorRightVector() * (YawRate * 0.00014);
			TargetLean = ((-Acceleration * 0.00007 - TurnLag) * MotionInertiaScale).GetClampedToMaxSize(0.12);
		}
	}
	DollMotion.PreviousFootContact = Contact;
	DollMotion.PreviousVelocity = PlanarVelocity;
	DollMotion.PreviousYaw = Yaw;
	DollMotion.bWasGrounded = bGrounded;

	FMath::SpringDamper(Pose.Compression, DollMotion.CompressionVelocity, 0.0f, 0.0f, DeltaSeconds, 4.2f, 0.60f);
	Pose.Compression = FMath::Clamp(Pose.Compression, -0.012f, 0.048f);
	FMath::SpringDamper(Pose.HeadCompression, DollMotion.HeadCompressionVelocity, Pose.Compression, 0.0f, DeltaSeconds, 3.0f, 0.8f);
	DollMotion.ContactHoldTime -= DeltaSeconds;
	const float ContactTarget = bGrounded && DollMotion.ContactHoldTime > 0.0f ? DollMotion.ContactHold : 0.0f;
	FMath::SpringDamper(Pose.ContactOffset, DollMotion.ContactVelocity, ContactTarget, 0.0f, DeltaSeconds, 4.1f, 0.62f);
	Pose.ContactOffset = FMath::Clamp(Pose.ContactOffset, -0.6f, 5.0f);
	FMath::SpringDamper(Pose.ChestLean, DollMotion.ChestLeanVelocity, TargetLean, FVector::ZeroVector, DeltaSeconds, 5.0f, 0.85f);
	FMath::SpringDamper(Pose.HeadLean, DollMotion.HeadLeanVelocity, Pose.ChestLean * 0.6, FVector::ZeroVector, DeltaSeconds, 3.0f, 0.8f);
}
