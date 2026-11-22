#include "Stage/Environment/StageControlMeshComponent.h"

#include "AnimationRuntime.h"
#include "Engine/SkinnedAsset.h"

UStageControlMeshComponent::UStageControlMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
}

void UStageControlMeshComponent::SetControlPose(FName Bone, const FQuat& Rotation, const FVector& Offset)
{
	const int32 Index = GetBoneIndex(Bone);
	if (!BoneSpaceTransforms.IsValidIndex(Index)) return;
	const FReferenceSkeleton& Skeleton = GetSkinnedAsset()->GetRefSkeleton();
	FTransform Pose = FAnimationRuntime::GetComponentSpaceTransform(Skeleton, Skeleton.GetRefBonePose(), Index);
	Pose.SetRotation(Rotation * Pose.GetRotation());
	Pose.AddToTranslation(Offset);
	SetBoneTransformByName(Bone, Pose, EBoneSpaces::ComponentSpace);
	// Socket attachments, including interaction grip points, follow the new pose immediately.
	RefreshBoneTransforms();
}
