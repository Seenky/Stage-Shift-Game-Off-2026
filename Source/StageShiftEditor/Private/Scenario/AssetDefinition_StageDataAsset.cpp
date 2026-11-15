#include "Scenario/AssetDefinition_StageDataAsset.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "StageScenarioEditorToolkit.h"

TSoftClassPtr<UObject> UAssetDefinition_StageDataAsset::GetAssetClass() const
{
	return UStageDataAsset::StaticClass();
}

EAssetCommandResult UAssetDefinition_StageDataAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UStageDataAsset* Data : OpenArgs.LoadObjects<UStageDataAsset>())
		MakeShared<FStageScenarioEditorToolkit>()->Init(Data, OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost);
	return EAssetCommandResult::Handled;
}
