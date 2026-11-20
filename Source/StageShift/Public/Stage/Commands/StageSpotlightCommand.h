#pragma once

#include "Stage/Core/StageMemberCommand.h"
#include "StageSpotlightCommand.generated.h"

UCLASS(meta = (DisplayName = "Spotlight"))
class STAGESHIFT_API UStageSpotlightCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual float GetNeededTime_Implementation() const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;

private:
	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (GetOptions = "GetTargetOptions"))
	FName SpotlightId;

	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (GetOptions = "GetRoleOptions"))
	FName TargetRoleId;

	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (ToolTip = "Character name shown on the cue card. Leave empty to use the role ID."))
	FText TargetDisplayName;

	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (ClampMin = "0.1", Units = "s"))
	float TimeLimit = 8.0f;
};
