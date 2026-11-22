#pragma once

#include "Stage/Actions/StageImpactAction.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StageImpactCommand.generated.h"

UCLASS(meta = (DisplayName = "Stage Impact"))
class STAGESHIFT_API UStageImpactCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	virtual uint8 GetResourceMask() const override { return Mode == EStageImpactMode::Collapse ? 0 : 3; }
	virtual bool CanRunWhileFallen() const override { return Mode == EStageImpactMode::Collapse; }
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual float GetNeededTime_Implementation() const override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;

private:
	AActor* ResolvePerformingTarget(const FStageExecutionContext& InContext) const;

	UPROPERTY(EditAnywhere, Category = "Scenario") EStageImpactMode Mode = EStageImpactMode::Strike;
	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (GetOptions = "GetRoleOptions", EditCondition = "Mode != EStageImpactMode::Collapse", EditConditionHides)) FName TargetRoleId;
	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (GetOptions = "GetTargetOptions", EditCondition = "Mode != EStageImpactMode::Collapse", EditConditionHides)) FName PropId;
	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (ClampMin = "0.0", Units = "s")) float ImpactDelay = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (ClampMin = "0.0", Units = "s", EditCondition = "Mode != EStageImpactMode::Collapse", EditConditionHides)) float StageTimeRequired = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (GetOptions = "GetTargetOptions", EditCondition = "Mode != EStageImpactMode::Collapse", EditConditionHides,
	    ToolTip = "Optional console used by a parallel Sound Cue targeting the same actor. An on-time signal triggers the action immediately. A missed cue skips a throw."))
	FName SyncWithSoundControlId;
	UPROPERTY(EditAnywhere, Category = "Animation", meta = (EditCondition = "Mode != EStageImpactMode::Collapse", EditConditionHides)) TObjectPtr<UAnimSequenceBase> WindupAnimation;
	UPROPERTY(EditAnywhere, Category = "Animation") TObjectPtr<UAnimMontage> FallMontage;
};
