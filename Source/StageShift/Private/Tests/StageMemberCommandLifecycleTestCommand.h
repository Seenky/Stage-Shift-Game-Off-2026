#pragma once

#include "Managers/Stage/StageMemberCommand.h"
#include "StageMemberCommandLifecycleTestCommand.generated.h"

UCLASS(Transient, NotBlueprintable)
class UStageMemberCommandLifecycleTestCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	bool bFinishDuringBegin = false;
	bool bDeferStop = false;
	int32 BeginCount = 0;
	int32 StopCount = 0;
	int32 NotificationCount = 0;
	EStageExecutionState StateDuringNotification = EStageExecutionState::NotStarted;
	EStageActionResult LastActionResult = EStageActionResult::None;

	UPROPERTY(Transient)
	float PlaybackPosition = 0.0f;

	void Complete(EStageCommandResult InResult) { Finish(InResult); }
	void SelectShortVersion() { UseShortVersion(); }

	UFUNCTION()
	void RecordFinished(UStageMemberCommand* Command, EStageCommandResult InResult);

protected:
	virtual float GetNeededTime_Implementation() const override;
	virtual float GetRemainingTime_Implementation() const override;
	virtual void BeginWork_Implementation() override;
	virtual void StopWork_Implementation(EStageActionStopReason Reason) override;
	virtual void OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult) override;
	virtual void ResetProgress_Implementation() override;
};
