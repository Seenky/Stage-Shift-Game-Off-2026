#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_StageShiftDollMotion.h"
#include "AnimGraphNode_StageShiftDollMotion.generated.h"

UCLASS()
class STAGESHIFTANIMGRAPH_API UAnimGraphNode_StageShiftDollMotion : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_StageShiftDollMotion Node;

	virtual FText GetTooltipText() const override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
