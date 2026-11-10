#pragma once

#include "Managers/Stage/StageAction.h"
#include "StageActionLifecycleTestAction.generated.h"

// Native test fixture: no game content or asset is created from this class.
UCLASS(Transient, NotBlueprintable)
class UStageActionLifecycleTestAction : public UStageAction
{
	GENERATED_BODY()

public:
	bool bFinishSynchronously = false;
	bool bDeferStop = false;
	EStageActionStopMode TestStopMode = EStageActionStopMode::StopAction;
	int32 ExecuteCount = 0;
	int32 StopCount = 0;
	int32 CleanupCount = 0;
	int32 NotificationCount = 0;
	EStageExecutionState StateDuringNotification = EStageExecutionState::NotStarted;
	EStageActionResult NotifiedResult = EStageActionResult::None;

	void Complete(EStageActionResult InResult) { Finish(InResult); }

	UFUNCTION()
	void RecordFinished(UStageAction* Action, EStageActionResult InResult);

protected:
	virtual void Execute_Implementation() override;
	virtual EStageActionStopMode GetStopMode_Implementation(EStageActionStopReason Reason) const override;
	virtual void StopNative_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;
};
