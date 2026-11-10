#include "Animation/AnimNode_StageShiftDollMotion.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/StageShiftAnimInstance.h"
#include "TwoBoneIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_StageShiftDollMotion)

namespace
{
	FQuat MakeDollLean(const FVector& Lean)
	{
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, Lean);
		const double Angle = Axis.Size();
		return Angle > SMALL_NUMBER ? FQuat(Axis / Angle, Angle) : FQuat::Identity;
	}
}

void FAnimNode_StageShiftDollMotion::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	Pelvis.Initialize(RequiredBones);
	Chest.Initialize(RequiredBones);
	Head.Initialize(RequiredBones);
	for (int32 Side = 0; Side < UE_ARRAY_COUNT(Feet); ++Side)
	{
		Clavicles[Side].Initialize(RequiredBones);
		UpperArms[Side].Initialize(RequiredBones);
		LowerArms[Side].Initialize(RequiredBones);
		Hands[Side].Initialize(RequiredBones);
		Thighs[Side].Initialize(RequiredBones);
		Calves[Side].Initialize(RequiredBones);
		Feet[Side].Initialize(RequiredBones);
	}
}

bool FAnimNode_StageShiftDollMotion::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	return Pelvis.IsValidToEvaluate(RequiredBones) && Chest.IsValidToEvaluate(RequiredBones) && Head.IsValidToEvaluate(RequiredBones);
}

void FAnimNode_StageShiftDollMotion::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const auto* Animation = Cast<UStageShiftAnimInstance>(Output.AnimInstanceProxy->GetAnimInstanceObject());
	if (!Animation) return;

	FStageShiftDollPose Motion = Animation->GetDollPose();
	const FStageShiftDragPose& Drag = Animation->GetDragPose();
	Motion.ChestLean += Drag.Lean * Drag.Alpha;
	Motion.Compression += Drag.Alpha * (0.008f + 0.01f * Drag.Effort);
	if (Animation->IsDollPhysicsActive())
	{
		// Lean and compression shape the motor targets; contacts are resolved by the simulated bodies.
		Motion.HeadLean *= 0.35;
		Motion.ContactOffset = 0.0f;
	}
	if (FMath::IsNearlyZero(Motion.Compression) && FMath::IsNearlyZero(Motion.HeadCompression) &&
		FMath::IsNearlyZero(Motion.ContactOffset) && Motion.ChestLean.IsNearlyZero() && Motion.HeadLean.IsNearlyZero() && Drag.Alpha <= 0.0f) return;

	const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
	const auto IndexOf = [&Bones](const FBoneReference& Bone) { return Bone.GetCompactPoseIndex(Bones); };
	const auto ReadBone = [&Output, &IndexOf](const FBoneReference& Bone) { return Output.Pose.GetComponentSpaceTransform(IndexOf(Bone)); };
	const auto WriteBone = [&OutBoneTransforms, &IndexOf](const FBoneReference& Bone, const FTransform& Transform)
	{
		OutBoneTransforms.Emplace(IndexOf(Bone), Transform);
	};
	const FTransform& Component = Output.AnimInstanceProxy->GetComponentTransform();
	FVector PelvisShift = FVector::ZeroVector;
	const bool bHasLegs = Thighs[0].IsValidToEvaluate(Bones) && Calves[0].IsValidToEvaluate(Bones) && Feet[0].IsValidToEvaluate(Bones) &&
		Thighs[1].IsValidToEvaluate(Bones) && Calves[1].IsValidToEvaluate(Bones) && Feet[1].IsValidToEvaluate(Bones);
	float DragLowering = 0.0f;
	if (bHasLegs && Drag.Alpha > 0.0f)
	{
		const float FloorHeight = FMath::Min(ReadBone(Feet[0]).GetLocation().Z, ReadBone(Feet[1]).GetLocation().Z);
		const float LegHeight = FMath::Max(ReadBone(Pelvis).GetLocation().Z - FloorHeight, 1.0);
		const float GripHeight = Component.InverseTransformPosition((Drag.Hands[0] + Drag.Hands[1]) * 0.5).Z;
		const float LowReach = FMath::Clamp((FloorHeight + LegHeight * 0.8f - GripHeight) / (LegHeight * 0.65f), 0.0f, 1.0f);
		DragLowering = LegHeight * 0.3f * LowReach * Drag.Alpha;
		Motion.ChestLean += Drag.Facing * (LowReach * 0.12f * Drag.Alpha);
	}
	if (bHasLegs && Motion.Compression > KINDA_SMALL_NUMBER)
	{
		PelvisShift = FVector::DownVector * (Motion.Compression * 92.0 + DragLowering);
		FTransform Hips = ReadBone(Pelvis);
		Hips.AddToTranslation(PelvisShift);
		WriteBone(Pelvis, Hips);

		// Preserve both incoming ankle targets and rotations before the terrain pass.
		for (int32 Side = 0; Side < UE_ARRAY_COUNT(Feet); ++Side)
		{
			FTransform Thigh = ReadBone(Thighs[Side]);
			FTransform Calf = ReadBone(Calves[Side]);
			FTransform Foot = ReadBone(Feet[Side]);
			const FTransform Target = Foot;
			const FVector LegAxis = (Foot.GetLocation() - Thigh.GetLocation()).GetSafeNormal();
			const FVector Bend = FVector::VectorPlaneProject(Calf.GetLocation() - Thigh.GetLocation(), LegAxis);
			Thigh.AddToTranslation(PelvisShift);
			Calf.AddToTranslation(PelvisShift);
			Foot.AddToTranslation(PelvisShift);
			const FVector Pole = Calf.GetLocation() + (Bend.IsNearlyZero(0.05) ? FVector(0, 20, 0) : FVector::ZeroVector);
			AnimationCore::SolveTwoBoneIK(Thigh, Calf, Foot, Pole, Target.GetLocation(), false, 1.0, 1.0);
			Foot.SetRotation(Target.GetRotation());
			WriteBone(Thighs[Side], Thigh);
			WriteBone(Calves[Side], Calf);
			WriteBone(Feet[Side], Foot);
		}
	}

	const FVector Away = Component.InverseTransformVectorNoScale(Motion.ContactNormal).GetSafeNormal2D();
	const FQuat ChestLean = MakeDollLean(Component.InverseTransformVectorNoScale(Motion.ChestLean));
	const FTransform InputChest = ReadBone(Chest);
	FTransform UpperBody = InputChest;
	UpperBody.AddToTranslation(PelvisShift + Away * (Motion.ContactOffset * 0.55) + FVector::DownVector * (Motion.Compression * 42.0));
	UpperBody.SetRotation(ChestLean * UpperBody.GetRotation());
	WriteBone(Chest, UpperBody);

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FBoneReference& Bone = Clavicles[Side];
		if (!Bone.IsValidToEvaluate(Bones)) continue;
		FTransform Shoulder = ReadBone(Bone).GetRelativeTransform(InputChest) * UpperBody;
		const FVector Outward = (Shoulder.GetLocation() - UpperBody.GetLocation()).GetSafeNormal2D();
		const double NearSide = FMath::Max(-FVector::DotProduct(Outward, Away), 0.0);
		Shoulder.AddToTranslation(Outward * (Motion.Compression * 22.0) + Away * (Motion.ContactOffset * NearSide * 0.20));
		WriteBone(Bone, Shoulder);
		if (Drag.Alpha > 0.0f && UpperArms[Side].IsValidToEvaluate(Bones) && LowerArms[Side].IsValidToEvaluate(Bones) && Hands[Side].IsValidToEvaluate(Bones))
		{
			const FTransform InputShoulder = ReadBone(Bone);
			FTransform Upper = ReadBone(UpperArms[Side]).GetRelativeTransform(InputShoulder) * Shoulder;
			FTransform Lower = ReadBone(LowerArms[Side]).GetRelativeTransform(InputShoulder) * Shoulder;
			FTransform Hand = ReadBone(Hands[Side]).GetRelativeTransform(InputShoulder) * Shoulder;
			const FVector Target = FMath::Lerp(Hand.GetLocation(), Component.InverseTransformPosition(Drag.Hands[Side]), Drag.Alpha);
			const FVector Right = Component.InverseTransformVectorNoScale(FVector::CrossProduct(FVector::UpVector, Drag.Facing));
			const FVector Down = Component.InverseTransformVectorNoScale(FVector::DownVector);
			const FVector Pole = Upper.GetLocation() + Right * (Side == 0 ? -35.0f : 35.0f) + Down * 20.0f;
			AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Pole, Target, false, 1.0, 1.0);
			WriteBone(UpperArms[Side], Upper);
			WriteBone(LowerArms[Side], Lower);
			WriteBone(Hands[Side], Hand);
		}
	}

	FTransform HeadTransform = ReadBone(Head).GetRelativeTransform(InputChest) * UpperBody;
	const FQuat HeadLean = MakeDollLean(Component.InverseTransformVectorNoScale(Motion.HeadLean));
	HeadTransform.AddToTranslation(FVector::DownVector * (Motion.HeadCompression * 15.0));
	HeadTransform.SetRotation(HeadLean * ChestLean.Inverse() * HeadTransform.GetRotation());
	WriteBone(Head, HeadTransform);
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
