#pragma once

#include "Tests/StageMemberCommandLifecycleTestCommand.h"
#include "StageContainerTestCommand.generated.h"

UCLASS(Transient, NotBlueprintable)
class UStageContainerTestCommand : public UStageMemberCommandLifecycleTestCommand
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float EstimatedDuration = 2.0f;

	UPROPERTY()
	TObjectPtr<UStageMemberCommand> ParentToStopOnBegin;

	UPROPERTY()
	TObjectPtr<UStageMemberCommand> ObservedRoot;

	bool bParentFinishedInsideBegin = false;
	bool bWholeTreeClosedDuringNotification = false;
	int32 FreezeCount = 0;

	UFUNCTION()
	void ObserveClosedTree(UStageMemberCommand* Command, EStageCommandResult Result);

protected:
	virtual float GetNeededTime_Implementation() const override { return EstimatedDuration; }
	virtual void BeginWork_Implementation() override;
	virtual void FreezeFinaleChoice_Implementation(const FStageExecutionContext& InContext) override { ++FreezeCount; }
};
