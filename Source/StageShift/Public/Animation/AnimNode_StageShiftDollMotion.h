#pragma once

#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_StageShiftDollMotion.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct STAGESHIFT_API FAnimNode_StageShiftDollMotion : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	FBoneReference Pelvis = FBoneReference(TEXT("pelvis"));
	FBoneReference Chest = FBoneReference(TEXT("spine_03"));
	FBoneReference Head = FBoneReference(TEXT("head"));
	FBoneReference Clavicles[2] = {FBoneReference(TEXT("clavicle_l")), FBoneReference(TEXT("clavicle_r"))};
	FBoneReference Thighs[2] = {FBoneReference(TEXT("thigh_l")), FBoneReference(TEXT("thigh_r"))};
	FBoneReference Calves[2] = {FBoneReference(TEXT("calf_l")), FBoneReference(TEXT("calf_r"))};
	FBoneReference Feet[2] = {FBoneReference(TEXT("foot_l")), FBoneReference(TEXT("foot_r"))};
};
