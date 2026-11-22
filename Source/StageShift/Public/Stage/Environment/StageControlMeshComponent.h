#pragma once

#include "Components/PoseableMeshComponent.h"
#include "StageControlMeshComponent.generated.h"

/** Visual only. The owning control retains its authored interaction and collision. */
UCLASS(ClassGroup = (Stage), meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UStageControlMeshComponent : public UPoseableMeshComponent
{
	GENERATED_BODY()

public:
	UStageControlMeshComponent(const FObjectInitializer& ObjectInitializer);

	// Offsets are measured from the reference pose, in the control's mesh space.
	void SetControlPose(FName Bone, const FQuat& Rotation, const FVector& Offset = FVector::ZeroVector);
};
