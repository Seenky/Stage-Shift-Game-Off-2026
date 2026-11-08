#include "Animation/StageShiftAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
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
	float TargetPickupAlpha = 0.0f;
	const UPhysicsManipulationComponent* Manipulation = Pawn ? Pawn->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	if (Manipulation)
	{
		Manipulation->GetPickupTarget(PickupTarget, TargetPickupAlpha, &PickupHandOffset);
	}
	// Reach follows the contact curve exactly; release eases out even after the montage ends.
	PickupAlpha = TargetPickupAlpha >= PickupAlpha ? TargetPickupAlpha
		: FMath::FInterpTo(PickupAlpha, TargetPickupAlpha, DeltaSeconds, 10.0f);
	if (!Manipulation || !Manipulation->GetManipulatedComponent() || (GetOwningComponent() && GetOwningComponent()->IsAnySimulatingPhysics())) PickupAlpha = 0.0f;
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
}
