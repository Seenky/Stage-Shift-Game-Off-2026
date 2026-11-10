#pragma once
#include "Managers/Stage/StageAction.h"
#include "Managers/Stage/StageMemberCommand.h"
#include "StageManagerTestTypes.generated.h"

class UStageDataAsset;
class UStageManagerTestCommand;

UCLASS(Transient, NotBlueprintable)
class UStageManagerTestRecorder : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<TObjectPtr<UStageManagerTestCommand>> Commands;
	UPROPERTY()
	TObjectPtr<AStageManager> Manager;
	UPROPERTY()
	TObjectPtr<UStageDataAsset> NextStage;
	UPROPERTY()
	TMap<FGameplayTag, AActor*> Actors;
	TSet<FName> Satisfied;
	int32 Completed = 0;
	int32 Failed = 0;
	int32 FinalScore = 0;
	bool bIdleDuringCompletion = false;
	bool bNextAccepted = false;
	UFUNCTION()
	void StageCompleted(FGameplayTag StageId);
	UFUNCTION()
	void StageFailed(FGameplayTag StageId, FText Reason);
	UFUNCTION()
	void AddAudienceScore(FStageEvent Event);
};

UCLASS(Transient, NotBlueprintable)
class UStageManagerTestAction : public UStageAction
{
	GENERATED_BODY()
public:
	UPROPERTY()
	bool bFinishFragment = false;
	UPROPERTY()
	bool bAutoFinish = false;
	UPROPERTY()
	bool bRecovery = false;
	void Complete(EStageActionResult Result) { Finish(Result); }
protected:
	virtual void Execute_Implementation() override;
	virtual EStageActionStopMode GetStopMode_Implementation(EStageActionStopReason Reason) const override;
};

UCLASS(Transient, NotBlueprintable)
class UStageManagerTestCommand : public UStageMemberCommand
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TObjectPtr<UStageManagerTestRecorder> Recorder;
	UPROPERTY()
	FName Label;
	UPROPERTY()
	float Duration = 2.0f;
	UPROPERTY()
	bool bAutoFinish = false;
	UPROPERTY()
	bool bFinishFragment = false;
	UPROPERTY(Transient)
	TObjectPtr<UStageManagerTestAction> LastAction;
	int32 ActionCount = 0;
protected:
	virtual float GetNeededTime_Implementation() const override { return Duration; }
	virtual void BeginWork_Implementation() override;
	virtual bool IsTaskSatisfied_Implementation(const FStageExecutionContext& Context) const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& Context) override;
	virtual void OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult Result) override;
};
