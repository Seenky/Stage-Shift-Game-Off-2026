#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stage/Core/Data/StageData.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "StageManager.generated.h"

class UStageDataAsset;
DECLARE_LOG_CATEGORY_EXTERN(LogStageShiftStage, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStageFinishedById, FName, StageKey, bool, bSucceeded, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStageProductionPhaseChanged, EStageProductionState, State, FName, SceneId, int32, SceneIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStageCompleted, FGameplayTag, StageId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStageFailed, FGameplayTag, StageId, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStageManagerEvent, FStageEvent, Event);

UCLASS(BlueprintType, Blueprintable)

class STAGESHIFT_API AStageManager : public AActor
{
	GENERATED_BODY()

public:
	AStageManager();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Production") TObjectPtr<UStageProductionDataAsset> ConfiguredProduction;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Production|Bindings") TMap<FName, FStageSceneBindings> ProductionBindings;
	UPROPERTY(BlueprintAssignable, Category = "Production") FStageProductionPhaseChanged OnProductionPhaseChanged;
	UPROPERTY(BlueprintAssignable, Category = "Production") FStageFinishedById OnProductionFinished;
	UFUNCTION(BlueprintCallable, Category = "Production") bool StartConfiguredProduction(FText& OutError);
	UFUNCTION(BlueprintCallable, Category = "Production") bool StartProduction(UStageProductionDataAsset* Production, FText& OutError);
	UFUNCTION(BlueprintCallable, Category = "Production") bool ConfirmProductionPreparation(FText& OutError);
	UFUNCTION(BlueprintCallable, Category = "Production") void AbortProduction(const FText& Reason);

	UFUNCTION(BlueprintPure, Category = "Production") EStageProductionState GetProductionState() const { return ProductionState; }

	UFUNCTION(BlueprintPure, Category = "Production") int32 GetProductionSceneIndex() const { return ProductionSceneIndex; }

	UFUNCTION(BlueprintPure, Category = "Production") int32 GetProductionSceneCount() const { return ProductionSceneCount; }

	UFUNCTION(BlueprintPure, Category = "Production") FText GetPreparationPrompt() const;
	UFUNCTION(BlueprintPure, Category = "Production") TArray<FString> GetPreparationErrors() const;
	UFUNCTION(CallInEditor, Category = "Production") void SynchronizeProductionBindings();
	UFUNCTION(CallInEditor, Category = "Production") void ValidateConfiguredProduction();
	void AddDiagnostic(const FString& Message);
	FString DescribeCommand(const UStageMemberCommand* Command) const;

	const TArray<FStageRootExecution>& GetRootExecutions() const { return RootExecutions; }
	UFUNCTION(BlueprintPure, Category = "Stage|Debug")

	TArray<FStageRootExecution> GetExecutionSnapshot() const { return RootExecutions; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Stage|Debug")
	TArray<FString> RecentEvents;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	TObjectPtr<UStageDataAsset> ConfiguredStage;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Bindings")
	TMap<FName, TObjectPtr<AActor>> RoleBindings;
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Bindings")
	TMap<FName, TObjectPtr<AActor>> TargetBindings;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario")
	bool bAutoStart = false;
	UPROPERTY(BlueprintAssignable, Category = "Scenario")
	FStageFinishedById OnStageFinishedById;
	UFUNCTION(CallInEditor, Category = "Bindings")
	void SynchronizeBindings();
	UFUNCTION(CallInEditor, Category = "Bindings")
	void ValidateConfiguredStage();
	UFUNCTION(BlueprintCallable, Category = "Scenario")
	bool StartConfiguredStage(FText& OutError);
	UFUNCTION(BlueprintCallable, Category = "Scenario", meta = (AutoCreateRefTerm = "MemberOverrides"))
	bool StartStageByIds(UStageDataAsset* StageData, const TMap<FName, AActor*>& MemberOverrides, FText& OutError);
	UFUNCTION(BlueprintPure, Category = "Bindings")
	AActor* GetParticipant(FName RoleId) const;
	UFUNCTION(BlueprintPure, Category = "Bindings")
	AActor* GetTarget(FName TargetId) const;
	bool CollectValidationErrors(UStageDataAsset* Data, const TMap<FName, AActor*>& Actors, TArray<FString>& Errors, bool bStrictRecovery = true) const;
	TMap<FName, AActor*> ResolveBindings(UStageDataAsset* Data, const TMap<FName, AActor*>& Overrides) const;
	UFUNCTION(BlueprintCallable, Category = "Scenario")
	void AbortStage(const FText& Reason);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TArray<int32> StarThresholds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TMap<EStageLatenessLevel, float> ScoreMultipliers;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage")
	int32 EstimatedTotalScore = 0;
	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FStageCompleted OnStageCompleted;
	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FStageFailed OnStageFailed;
	UPROPERTY(BlueprintAssignable, Category = "Stage")
	FStageManagerEvent OnStageEvent;

	UFUNCTION(BlueprintCallable, Category = "Stage", meta = (AutoCreateRefTerm = "MemberOverrides"))
	bool StartStage(UStageDataAsset* StageData, const TMap<FGameplayTag, AActor*>& MemberOverrides, FText& OutError);
	UFUNCTION(BlueprintPure, Category = "Stage")
	AActor* GetMember(FGameplayTag MemberTag) const;
	UFUNCTION(BlueprintPure, Category = "Stage")

	EStageState GetState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "Stage")
	float GetStageElapsedTime() const;
	UFUNCTION(BlueprintPure, Category = "Stage")

	UStageDataAsset* GetActiveStage() const { return ActiveStage; }
	UFUNCTION(BlueprintPure, Category = "Stage")

	EStageLatenessLevel GetCurrentLatenessLevel() const { return CurrentLatenessLevel; }
	UFUNCTION(BlueprintCallable, Category = "Stage|Score")
	void AddScore(int32 Amount);
	UFUNCTION(BlueprintPure, Category = "Stage|Score")

	int32 GetScore() const { return Score; }
	UFUNCTION(BlueprintPure, Category = "Stage|Score")
	int32 GetStarCount() const;
	UFUNCTION(BlueprintCallable, Category = "Stage")
	void ReceiveStageEvent(const FStageEvent& Event);
	UFUNCTION(CallInEditor, Category = "Stage|Score")
	void RecalculateEstimatedScore();
	virtual void Tick(float DeltaSeconds) override;

private:
	bool IsProductionActive() const { return ProductionState == EStageProductionState::Preparing || ProductionState == EStageProductionState::Playing; }

	bool StartStageWithBindings(UStageDataAsset* Data, const TMap<FName, AActor*>& Roles, const TMap<FName, TObjectPtr<AActor>>& Targets, FText& Error, bool bStrictRecovery);
	struct FResolvedSceneBindings
	{
		TMap<FName, TObjectPtr<AActor>> Roles;
		TMap<FName, TObjectPtr<AActor>> Targets;
	};
	FResolvedSceneBindings ResolveProductionBindings(const FStageProductionScene& Scene) const;
	bool ValidateProduction(UStageProductionDataAsset* Production, TArray<FString>& Errors) const;
	bool StartProductionScene(FText& Error);
	void AdvanceProduction();
	void HandleProductionSceneFinished(FName StageKey, bool bSucceeded, const FText& Reason);
	void FinishProduction(bool bSucceeded, const FText& Reason);
	void TickProduction();
	UPROPERTY(Transient) TObjectPtr<UStageProductionDataAsset> ActiveProduction;
	EStageProductionState ProductionState = EStageProductionState::Idle;
	int32 ProductionSceneIndex = INDEX_NONE;
	int32 ProductionSceneCount = 0;
	double PreparationStartedAt = 0;
	bool bStartingProductionScene = false;
	bool bAbortingProduction = false;
	bool bManagerEndingPlay = false;
	bool bProductionAdvancePending = false;
	FGuid ProductionRunId;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<AActor>> ActiveTargetBindings;
	mutable bool bUseValidationTargets = false;
	mutable TMap<FName, TObjectPtr<AActor>> ValidationTargets;
	friend class UStageMemberComponent;
	friend class UStageMemberCommand;
	bool ValidateStage(UStageDataAsset* Data, const TMap<FName, AActor*>& Actors, FText& Error) const;
	UStageMemberCommand* CreatePreparedCommand(const FStageMemberAssignment& Assignment, FText& Error);
	TArray<TObjectPtr<UStageMemberCommand>> PrepareCommands(
	    const TArray<FStageMemberAssignment>& Assignments, EStageCommandType Type, int32 SourceIndex, FGuid ReactionActivation = FGuid());
	void IssueCommands(const TArray<TObjectPtr<UStageMemberCommand>>& Commands);
	void AdvanceStage();
	void BeginFinale();
	void FailStage(const FText& Reason);
	void FinishStageIfReady();
	void CleanupStage();
	void UpdateTiming();
	void UpdateDebugDisplay();
	void RemoveDebugDisplay();
	TSharedPtr<class SStageDebugWidget> DebugWidget;
	TSharedPtr<class SWindow> DebugWindow;
#if WITH_EDITOR
	TWeakPtr<class SDockTab> DebugTab;
	FName DebugTabId;
#endif
	float GetCommandLateness(const UStageMemberCommand* Command) const;
	EStageLatenessLevel ResolveLatenessLevel(float Seconds) const;
	FStageRootExecution* FindRoot(const UStageMemberCommand* Command);
	const FStageRootExecution* FindRoot(const UStageMemberCommand* Command) const;
	EStageCommandResult CheckCommandStart(UStageMemberCommand* Command);
	bool ShouldSkipCommand(const UStageMemberCommand* Command) const;
	bool CheckReaction(const FStageRootExecution& Entry) const;
	void HandleCommandStarted(UStageMemberCommand* Command);
	void AccountCommandResult(UStageMemberCommand* Command, EStageCommandResult Result);
	void BeginNotification();
	void EndNotification();
	UFUNCTION()
	void HandleRootFinished(UStageMemberCommand* Command, EStageCommandResult Result);
	UFUNCTION()
	void HandleMemberFinished(UStageMemberComponent* Member);

	EStageState State = EStageState::Idle;
	UPROPERTY(Transient)
	TObjectPtr<UStageDataAsset> ActiveStage;
	int32 Score = 0;
	float StageElapsedTime = 0.0f;
	int32 CurrentStepIndex = INDEX_NONE;
	float CurrentLatenessSeconds = 0.0f;
	float MaxLatenessSeconds = 0.0f;
	EStageLatenessLevel CurrentLatenessLevel = EStageLatenessLevel::OnTime;
	TMap<FName, TWeakObjectPtr<AActor>> Members;
	TArray<TWeakObjectPtr<UStageMemberComponent>> BoundMembers;
	UPROPERTY(Transient)
	TArray<FStageRootExecution> RootExecutions;
	UPROPERTY(Transient)
	TArray<FStageStep> PreparedSteps;
	TArray<float> PlannedStepStarts;
	TMap<FName, EStageCommandResult> PreviousStepResults;
	TMap<FName, EStageCommandResult> CurrentStepResults;
	TArray<int32> ReactionActivationCounts;
	TArray<double> LastReactionStartedTimes;
	TSet<FGuid> StartedReactionActivations;
	double FinalizingStartedAt = 0.0;
	double StoppingDeadline = 0.0;
	FGuid RunId;
	TSet<TWeakObjectPtr<UStageMemberCommand>> AccountedCommands;
	FText FailureReason;
	double StageStartedAt = 0.0;
	int32 EventDispatchDepth = 0;
	bool bIssuingCommands = false;
	bool bAdvancing = false;
	bool bAdvanceRequested = false;
};
