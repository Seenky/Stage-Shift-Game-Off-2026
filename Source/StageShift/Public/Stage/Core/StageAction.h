#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/Data/StageData.h"
#include "UObject/Object.h"
#include "StageAction.generated.h"

class UStageAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStageActionFinished, UStageAction*, Action, EStageActionResult, Result);

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)

class STAGESHIFT_API UStageAction : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Advanced", meta = (ClampMin = "0.1", Units = "s"))
	float WatchdogSeconds = 30.0f;
	void SetExecutionBudget(float Seconds);
	UFUNCTION(BlueprintCallable, Category = "Stage|Action")
	void ForceAbort(const FText& Reason);

	const FText& GetTerminationReason() const { return TerminationReason; }

	virtual void ValidateRecoveryTemplate(const FString& Path, TArray<FString>& Errors) const;
	UFUNCTION(BlueprintCallable, Category = "Stage|Action")
	void Start(const FStageExecutionContext& InContext);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Action")
	bool CanContinue() const;

	UFUNCTION(BlueprintCallable, Category = "Stage|Action")
	void RequestStop(EStageActionStopReason Reason);

	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintPure, Category = "Stage|Action")

	EStageExecutionState GetState() const { return ActionState; }
	UFUNCTION(BlueprintPure, Category = "Stage|Action")

	EStageActionResult GetResult() const { return ActionResult; }

	UPROPERTY(BlueprintAssignable, Category = "Stage|Action")
	FStageActionFinished OnFinished;

protected:
	UPROPERTY()
	FStageExecutionContext Context;

	UFUNCTION(BlueprintCallable, Category = "Stage|Action")
	void Finish(EStageActionResult InResult);

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Action")
	void OnStart();

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Action")
	EStageActionStopMode GetStopMode(EStageActionStopReason Reason) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Action")
	void OnStop(EStageActionStopMode Mode);

	/*
	 * Called on finish()
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Action")
	void Cleanup();

private:
	FTimerHandle WatchdogHandle;
	FText TerminationReason;
	void HandleWatchdog();
	EStageExecutionState ActionState = EStageExecutionState::NotStarted;
	EStageActionResult ActionResult = EStageActionResult::None;
	EStageActionStopReason ActionStopReason = EStageActionStopReason::Interrupt;
	EStageActionStopMode ActionStopMode = EStageActionStopMode::StopAction;
};
