#pragma once
#include "CoreMinimal.h"

class UStageProductionDataAsset;
class UStageScenarioEdGraph;
class UStageScenarioEdNode;

struct FStageProductionGraphAdapter
{
	explicit FStageProductionGraphAdapter(UStageProductionDataAsset* InData) : Data(InData) {}

	void Build(UStageScenarioEdGraph* Graph) const;
	void EnsureIdentities();
	static FGuid SceneKey(const UStageProductionDataAsset* Data, int32 Index, bool bPreparation = false);
	bool Add(int32 After = INDEX_NONE);
	bool AddInView(UStageScenarioEdGraph* Graph, const UStageScenarioEdNode* From, FVector2D Position);
	bool Preparation(int32 Index, bool bEnable);
	bool Delete(const UStageScenarioEdNode* Node);
	bool MoveAfter(const UStageScenarioEdGraph* Graph, const UStageScenarioEdNode* Node, const UStageScenarioEdNode* After);
	UStageProductionDataAsset* Data;
};
