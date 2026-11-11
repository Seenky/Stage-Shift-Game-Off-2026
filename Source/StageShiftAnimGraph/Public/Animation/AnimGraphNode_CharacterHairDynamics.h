#pragma once

#include "AnimGraphNode_Base.h"
#include "Animation/AnimNode_CharacterHairDynamics.h"
#include "AnimGraphNode_CharacterHairDynamics.generated.h"

UCLASS()
class STAGESHIFTANIMGRAPH_API UAnimGraphNode_CharacterHairDynamics : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_CharacterHairDynamics Node;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
};
