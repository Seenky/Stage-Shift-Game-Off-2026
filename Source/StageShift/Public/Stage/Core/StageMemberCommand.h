#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/Data/StageData.h"
#include "UObject/Object.h"
#include "StageMemberCommand.generated.h"

class UStageMemberCommand;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStageCommandFinished, UStageMemberCommand*, Command, EStageCommandResult, Result);

// A task and its progress. Concrete engine work belongs to UStageAction.
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class STAGESHIFT_API UStageMemberCommand : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Command")
	EStageCatchUpPolicy CatchUpPolicy = EStageCatchUpPolicy::Keep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Command")
	FStageExecutionRequirements ExecutionRequirements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Command")
	FGameplayTag RequiredSuccessStepMemberTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Scoring", meta = (ClampMin = "0", UIMin = "0"))
	int32 ScoreValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage|Command")
	FGameplayTag OnSuccessEventTag;

	UPROPERTY(BlueprintAssignable, Category = "Stage|Command")
	FStageCommandFinished OnFinished;

	UFUNCTION(BlueprintCallable, Category = "Stage|Command")
	void Start(const FStageExecutionContext& InContext);
	
	UFUNCTION(BlueprintCallable, Category = "Stage|Command")
	void RequestStop(EStageActionStopReason Reason);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	float GetNeededTime() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	bool IsTaskSatisfied(const FStageExecutionContext& InContext) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Stage|Command")
	UStageAction* CreateMainAction(const FStageExecutionContext& InContext);
	
	void NotifyMainActionStarted();
	void ApplyMainActionResult(UStageAction* Action, EStageActionResult ActionResult);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	FStageEvent BuildSuccessEvent() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Stage|Command")
	void FreezeFinaleChoice(const FStageExecutionContext& InContext);
	
	virtual void ResetRuntime( 
		const FStageExecutionContext& InContext, 
		UStageMemberCommand* InRoot,
		float InPlannedStartTime, float InPlannedEndTime);

	virtual bool IsComposite() const { return false; }
	virtual const TArray<TObjectPtr<UStageMemberCommand>>& GetChildCommands() const;
	virtual UWorld* GetWorld() const override;

	EStageExecutionState GetState() const { return CommandState; }
	EStageCommandResult GetResult() const { return CommandResult; }
	UStageMemberCommand* GetRootCommand() const { return RootCommand.Get(); }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")
	float GetPlannedStartTime() const { return PlannedStartTime; }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")
	float GetPlannedEndTime() const { return PlannedEndTime; }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")
	float GetAccumulatedActiveTime() const;

	UFUNCTION(BlueprintPure, Category = "Stage|Command")
	bool IsUsingShortVersion() const { return bUseShortVersion; }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")
	bool HasSuccessfulMainAction() const { return bHasSuccessfulMainAction; }

protected:
	UPROPERTY()
	FStageExecutionContext Context;

	UFUNCTION(BlueprintCallable, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void Finish(EStageCommandResult InResult);

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void BeginWork();

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void StopWork(EStageActionStopReason Reason);

	// Base timing is saved first; a subclass then reads its own progress from Action.
	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnMainActionResult(UStageAction* Action, EStageActionResult ActionResult);

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void ResetProgress();

	UFUNCTION(BlueprintCallable, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void UseShortVersion();

private:
	friend class UStageMemberComponent;

	void SaveAccumulatedActiveTime();

	EStageExecutionState CommandState = EStageExecutionState::NotStarted;
	EStageCommandResult CommandResult = EStageCommandResult::None;

	UPROPERTY(Transient)
	TWeakObjectPtr<UStageMemberCommand> RootCommand;

	float PlannedStartTime = 0.0f;
	float PlannedEndTime = 0.0f;
	// Accumulated time of main action
	float AccumulatedActiveTime = 0.0f;
	// Start time of current main action
	float CurrentStartTime = -1.0f;
	bool bUseShortVersion = false;
	bool bHasSuccessfulMainAction = false;
};
