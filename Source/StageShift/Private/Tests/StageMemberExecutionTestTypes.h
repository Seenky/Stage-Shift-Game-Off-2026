#pragma once

#include "Managers/Stage/StageAction.h"
#include "Managers/Stage/StageMemberCommand.h"
#include "StageMemberExecutionTestTypes.generated.h"

UCLASS(Transient, NotBlueprintable)
class UStageMemberExecutionTestAction : public UStageAction
{
	GENERATED_BODY()

public:
	UPROPERTY()
	bool bAutoFinish = false;

	UPROPERTY()
	bool bRemoveStatusOnStart = false;

	UPROPERTY()
	bool bDeferStop = false;

	int32 StopCount = 0;
	void Complete(EStageActionResult InResult) { Finish(InResult); }

protected:
	virtual void Execute_Implementation() override;
	virtual void StopNative_Implementation(EStageActionStopMode Mode) override;
};

UCLASS(Transient, NotBlueprintable)
class UStageMemberExecutionTestCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	bool bAutoFinishMain = false;
	bool bTaskAlreadySatisfied = false;
	bool bEndMemberOnReport = false;
	bool bMemberFinishedInsideCallback = false;
	int32 CreateCount = 0;
	EStageActionResult LastMainResult = EStageActionResult::None;

	UPROPERTY()
	TObjectPtr<UStageMemberExecutionTestAction> LastMainAction;

	UFUNCTION()
	void HandleCommandReported(UStageMemberCommand* Command, EStageCommandResult Result);

	UFUNCTION()
	void HandleOwnFinished(UStageMemberCommand* Command, EStageCommandResult Result);

protected:
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual bool IsTaskSatisfied_Implementation(const FStageExecutionContext& InContext) const override;
	virtual void OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult Result) override;
};
