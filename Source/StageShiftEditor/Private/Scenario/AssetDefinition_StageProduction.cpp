#include "Scenario/AssetDefinition_StageProduction.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "StageScenarioEditorToolkit.h"
TSoftClassPtr<UObject> UAssetDefinition_StageProduction::GetAssetClass() const{return UStageProductionDataAsset::StaticClass();}
EAssetCommandResult UAssetDefinition_StageProduction::OpenAssets(const FAssetOpenArgs& Args) const
{for(auto* Asset:Args.LoadObjects<UStageProductionDataAsset>())MakeShared<FStageScenarioEditorToolkit>()->Init(Asset,Args.GetToolkitMode(),Args.ToolkitHost);return EAssetCommandResult::Handled;}
