#pragma once

#include "Animation/AnimNodeBase.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "BoneControllers/AnimNode_AnimDynamics.h"
#include "AnimNode_CharacterHairDynamics.generated.h"

// Builds a graph of stock AnimDynamics nodes. There is no custom physics solver.
USTRUCT(BlueprintInternalUseOnly)
struct STAGESHIFT_API FAnimNode_CharacterHairDynamics : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink SourcePose;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* Instance) override;
	virtual bool NeedsDynamicReset() const override { return true; }
	virtual void ResetDynamics(ETeleportType Type) override;
	int32 GetSimulatedStrandCount() const { return Dynamics.Num(); }

private:
	FAnimNode_ConvertLocalToComponentSpace ToComponent;
	TArray<FAnimNode_AnimDynamics> Dynamics;
	FAnimNode_ConvertComponentToLocalSpace ToLocal;
};
