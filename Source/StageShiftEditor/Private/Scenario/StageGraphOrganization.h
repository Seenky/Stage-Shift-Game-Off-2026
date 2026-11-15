#pragma once
#include "CoreMinimal.h"
class UStageScenarioEdGraph;
class UStageScenarioEdNode;
class FStageGraphOrganization
{
public:
    static void Project(UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Logical);
    static bool CanCollapse(const UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Nodes);
    static bool Collapse(UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Nodes, const FString& Name);
    static bool Expand(UStageScenarioEdGraph* Graph, FGuid GroupId);
    static bool Rename(UStageScenarioEdGraph* Graph, UStageScenarioEdNode* Node, const FString& Name);
    static bool Delete(UStageScenarioEdGraph* Graph,const TArray<UStageScenarioEdNode*>& Nodes);
    static void Layout(UStageScenarioEdGraph* Graph);
};
