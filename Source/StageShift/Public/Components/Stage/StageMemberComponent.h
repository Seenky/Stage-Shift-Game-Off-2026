#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Stage/Core/Data/StageData.h"
#include "StageMemberComponent.generated.h"

class UStageAction;
class UStageMemberCommand;
class UStageRecoveryRulesDataAsset;
class UStatusesComponent;
class UStageMemberComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStageMemberCommandFinished, UStageMemberCommand*, Command, EStageCommandResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStageMemberEvent, FStageEvent, Event);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStageMemberFinished, UStageMemberComponent*, Member);

// Executes one actor's command roots and their leaf Actions.
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Stage), meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UStageMemberComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStageMemberComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<UStageRecoveryRulesDataAsset> RecoveryRulesAsset;

	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FStageMemberCommandFinished OnCommandFinished;

	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FStageMemberEvent OnStageEvent;

	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FStageMemberFinished OnMemberFinished;

	UFUNCTION(BlueprintCallable, Category = "Stage")
	bool BindToStage(AStageManager* InManager);

	// Common entry for a prepared root or container child.
	bool RunCommand(UStageMemberCommand* Command, EStageCommandType Type);
	bool HasReaction() const;
	void RequestStopCommands();
	void StartPreparedCommand(UStageMemberCommand* Command);
	void CompleteBeforeStart(UStageMemberCommand* Command, EStageCommandResult Result);

	void BeginExecution(UStageMemberCommand* Command);
	void StopExecution(UStageMemberCommand* Command, EStageActionStopReason Reason);

	UFUNCTION(BlueprintPure, Category = "Stage")
	UStageAction* GetMainAction(UStageMemberCommand* Command) const;

	UFUNCTION(BlueprintPure, Category = "Stage")
	UStageAction* GetRecoveryAction() const { return ActiveRecoveryAction; }

	UFUNCTION(BlueprintCallable, Category = "Stage")
	void ReportStageEvent(const FStageEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "Stage")
	void EndStageMember();

	UFUNCTION(BlueprintPure, Category = "Stage")
	bool IsMemberFinished() const { return bMemberFinished; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend class UStageAction;
	friend class UStageMemberCommand;

	void BeginDispatch();
	void EndDispatch();
	void HandleCommandFinished(UStageMemberCommand* Command, EStageCommandResult Result);
	void PumpCommands();
	void UpdateExecutions();
	void AdvanceExecution(UStageMemberCommand* Command);
	void FinishExecution(UStageMemberCommand* Command, EStageCommandResult Result);
	void RemoveRecoveryWaiter(UStageMemberCommand* Command);
	void TryFinishMember();
	FStageCommandExecution* FindExecution(UStageMemberCommand* Command);
	UStageAction* SelectRecoveryTemplate() const;

	UFUNCTION()
	void HandleActionFinished(UStageAction* Action, EStageActionResult Result);

	UFUNCTION()
	void HandleStatusesChanged(FGameplayTagContainer ChangedStatuses);

	UPROPERTY(Transient)
	TWeakObjectPtr<AStageManager> Manager;

	UPROPERTY(Transient)
	TWeakObjectPtr<UStatusesComponent> StatusComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStageMemberCommand>> RootCommands;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStageMemberCommand>> PendingCommands;

	UPROPERTY(Transient)
	TObjectPtr<UStageMemberCommand> PendingReaction;

	UPROPERTY(Transient)
	TArray<FStageCommandExecution> Executions;

	UPROPERTY(Transient)
	TObjectPtr<UStageAction> ActiveRecoveryAction;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStageMemberCommand>> RecoveryWaiters;

	// Retain a finished Action until all nested completion broadcasts return.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStageAction>> FinishingActions;

	FGameplayTagContainer CachedStatuses;
	int32 DispatchDepth = 0;
	bool bUpdatingExecutions = false;
	bool bUpdateRequested = false;
	bool bPumpingCommands = false;
	bool bClosingMember = false;
	bool bMemberFinished = true;
};
