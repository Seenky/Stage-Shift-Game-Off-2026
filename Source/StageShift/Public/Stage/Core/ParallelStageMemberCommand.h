#pragma once

#include "Stage/Core/StageMemberCommand.h"
#include "ParallelStageMemberCommand.generated.h"

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, HideCategories = (Scoring))

class STAGESHIFT_API UParallelStageMemberCommand final : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Stage|Command")
	TArray<TObjectPtr<UStageMemberCommand>> Children;

	UPROPERTY(EditAnywhere, Category = "Parallel", meta = (ToolTip = "Continue when the selected primary branch ends. Other branches are cancelled."))
	bool bFinishOnPrimaryBranch = false;

	UPROPERTY(EditAnywhere, Category = "Parallel", meta = (NoGraphParameter, EditCondition = "false", EditConditionHides))
	TObjectPtr<UStageMemberCommand> PrimaryBranch;

	virtual bool IsComposite() const override { return true; }

	virtual const TArray<TObjectPtr<UStageMemberCommand>>& GetChildCommands() const override { return Children; }

	virtual void ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot, float InPlannedStartTime, float InPlannedEndTime) override;
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual float GetNeededTime_Implementation() const override;
	virtual float GetRemainingTime_Implementation() const override;
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopReason Reason) override;

private:
	void CheckChildrenResults();
	void StopChildren(EStageActionStopReason Reason);
	void TryFinish();

	UFUNCTION()
	void HandleChildFinished(UStageMemberCommand* Command, EStageCommandResult Result);

	TArray<EStageCommandResult> ChildResults;
	bool bIssuingChildren = false;
};
