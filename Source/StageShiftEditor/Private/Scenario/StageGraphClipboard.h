#pragma once
#include "CoreMinimal.h"

class UStageScenarioEdGraph;
class UStageScenarioEdNode;

struct FStageGraphClipboard
{
	static bool Copy(UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Nodes, FString& Text);
	static bool Paste(UStageScenarioEdGraph* Graph, const FString& Text, FVector2D Location, bool bDisconnected = false);
};
