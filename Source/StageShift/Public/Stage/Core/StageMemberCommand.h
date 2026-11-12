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

	void PreStart(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Stage|Command")
	void Start(const FStageExecutionContext& InContext);
	
	UFUNCTION(BlueprintCallable, Category = "Stage|Command")
	void RequestStop(EStageActionStopReason Reason);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	float GetNeededTime() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	bool IsResultAchieved(const FStageExecutionContext& InContext) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Stage|Command")
	UStageAction* CreateMainAction(const FStageExecutionContext& InContext);
	
	void NotifyMainActionStarted();
	void ApplyMainActionResult(UStageAction* Action, EStageActionResult ActionResult);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	FStageEvent BuildSuccessEvent() const;

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
	bool HasStartedMainAction() const { return bHasStartedMainAction; }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")
	bool HasSuccessfulMainAction() const { return bHasSuccessfulMainAction; }

protected:
	UPROPERTY()
	FStageExecutionContext Context;

	UFUNCTION(BlueprintCallable, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void Finish(EStageCommandResult InResult);

	// Check before starting or resuming Main; switching remains explicit through UseShortVersion().
	UFUNCTION(BlueprintPure, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	bool ShouldUseShortVersion(float ShortDuration) const;

	UFUNCTION(BlueprintCallable, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void UseShortVersion();
	
	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnPreStart(const FStageExecutionContext& InContext);

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnStart();

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnStop(EStageActionStopReason Reason);

/*
Called every time when main action call finish
(preferred to save progress)
*/
	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnMainActionResult(UStageAction* Action, EStageActionResult ActionResult);

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
	bool bPreStarted = false;
	bool bUseShortVersion = false;
	bool bHasStartedMainAction = false;
	bool bHasSuccessfulMainAction = false;
};
