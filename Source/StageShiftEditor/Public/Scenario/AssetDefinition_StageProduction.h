#pragma once
#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "AssetDefinition_StageProduction.generated.h"
UCLASS()
class UAssetDefinition_StageProduction : public UAssetDefinitionDefault
{
    GENERATED_BODY()
public:
    virtual FText GetAssetDisplayName() const override{return NSLOCTEXT("StageScenario","ProductionAsset","Stage Production");}
    virtual FLinearColor GetAssetColor() const override{return FLinearColor(0.3f,0.22f,0.6f);}
    virtual TSoftClassPtr<UObject> GetAssetClass() const override;
    virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& Args) const override;
};
