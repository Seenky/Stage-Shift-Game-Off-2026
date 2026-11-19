#pragma once
#include "CoreMinimal.h"

class UStageScenarioEdGraph;
class UStageScenarioEdNode;
class UClass;

struct FStageGraphDrafts
{
	static FGuid EventId(FGuid Id) { return FGuid(Id.A, Id.B, Id.C, Id.D ^ 0xf00d); }

	static bool Add(UStageScenarioEdGraph* Graph, UClass* Class, const UStageScenarioEdNode* Context, FVector2D Position);
	static bool AddBranch(UStageScenarioEdGraph* Graph, const UStageScenarioEdNode* Context, FVector2D Position);
	static bool AddScene(UStageScenarioEdGraph* Graph, FVector2D Position);
	static bool AddPreparation(UStageScenarioEdGraph* Graph, const UStageScenarioEdNode* Scene, FVector2D Position);
	static void Project(UStageScenarioEdGraph* Graph);
	static bool Connect(UStageScenarioEdGraph* Graph, const UStageScenarioEdNode* To, const UStageScenarioEdNode* From, bool bOnFailed = false);
	static bool Delete(UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Nodes);
	static bool SetRole(UStageScenarioEdGraph* Graph, const UStageScenarioEdNode* Node, FName Role, bool bSubject);
};
