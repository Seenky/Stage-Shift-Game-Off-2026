#pragma once
#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "AssetDefinition_StageDataAsset.generated.h"
UCLASS()
class STAGESHIFTEDITOR_API UAssetDefinition_StageDataAsset : public UAssetDefinitionDefault
{
    GENERATED_BODY()
public:
    virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("StageScenario", "AssetName", "Stage Scenario"); }
    virtual FLinearColor GetAssetColor() const override { return FLinearColor(0.1f, 0.55f, 0.65f); }
    virtual TSoftClassPtr<UObject> GetAssetClass() const override;
    virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
