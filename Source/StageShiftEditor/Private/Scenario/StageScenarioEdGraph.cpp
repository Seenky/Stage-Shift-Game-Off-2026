#include "Scenario/StageScenarioEdGraph.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"

UObject* UStageScenarioEdGraph::GetAsset() const
{
	return Scenario ? static_cast<UObject*>(Scenario) : Production.Get();
}

bool UStageScenarioEdGraph::IsStorySteps() const
{
	return Scenario && !Production && !bReactions && !ScopeCommand && !bParallelAssignments && !bReactionAssignments;
}

TMap<FGuid, FVector2D>& UStageScenarioEdGraph::Positions() const
{
	return Scenario ? Scenario->EditorPositions : Production->EditorPositions;
}

TArray<FStageGraphGroup>& UStageScenarioEdGraph::Groups() const
{
	return Scenario ? Scenario->EditorGroups : Production->EditorGroups;
}

TMap<FGuid, FStageGraphViewState>& UStageScenarioEdGraph::Views() const
{
	return Scenario ? Scenario->EditorViews : Production->EditorViews;
}

FGuid UStageScenarioEdGraph::ViewKey() const
{
	return FGuid(ScopeId.A ^ ActiveGroup.A, ScopeId.B ^ ActiveGroup.B, ScopeId.C ^ ActiveGroup.C, ScopeId.D ^ ActiveGroup.D);
}
