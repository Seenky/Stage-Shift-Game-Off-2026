#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "StageGraphClipboardNode.generated.h"
// Export-only owned snapshot; editor views never store canonical command data here.
UCLASS(Transient)
class UStageGraphClipboardNode : public UEdGraphNode
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 Mode=0;
    UPROPERTY() TArray<FStageStep> Steps;
    UPROPERTY() TArray<FStageMemberReaction> Reactions;
    UPROPERTY(Instanced) TArray<TObjectPtr<UStageMemberCommand>> Commands;
    UPROPERTY() TArray<FStageMemberAssignment> Assignments;
    UPROPERTY() TArray<FStageProductionScene> Scenes;
    UPROPERTY() TArray<FStageGraphGroup> Groups;
    UPROPERTY() FInstancedPropertyBag Parameters;
};
