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
	UFUNCTION(BlueprintCallable, Category = "Stage|Action")
	void Start(const FStageExecutionContext& InContext);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Action")
	bool CanContinue() const;

	UFUNCTION(BlueprintCallable, Category = "Stage|Action")
	void RequestStop(EStageActionStopReason Reason);

	virtual UWorld* GetWorld() const override;

	EStageExecutionState GetState() const { return ActionState; }
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
	EStageExecutionState ActionState = EStageExecutionState::NotStarted;
	EStageActionResult ActionResult = EStageActionResult::None;
	EStageActionStopReason ActionStopReason = EStageActionStopReason::Interrupt;
	EStageActionStopMode ActionStopMode = EStageActionStopMode::StopAction;
};
