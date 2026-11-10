#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Managers/Stage/Data/StageData.h"
#include "StageManager.generated.h"



class UStageDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStageCompleted, FGameplayTag, StageId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStageFailed, FGameplayTag, StageId, FText, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStageManagerEvent, FStageEvent, Event);

UCLASS(BlueprintType, Blueprintable)
class STAGESHIFT_API AStageManager : public AActor
{
	GENERATED_BODY()

public:
	AStageManager();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TArray<TObjectPtr<UStageDataAsset>> StageDefinitions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Score")
	TArray<int32> StarThresholds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Score")
	TMap<EStageLatenessLevel, float> ScoreMultipliers;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage|Score")
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
	friend class UStageMemberComponent;
	bool ValidateStage(UStageDataAsset* Data, const TMap<FGameplayTag, AActor*>& Actors, FText& Error) const;
	TArray<TObjectPtr<UStageMemberCommand>> PrepareCommands(const TArray<FStageMemberAssignment>& Assignments,
		EStageCommandType Type, int32 SourceIndex);
	void IssueCommands(const TArray<TObjectPtr<UStageMemberCommand>>& Commands);
	void AdvanceStage();
	void BeginFinale();
	void FailStage(const FText& Reason);
	void FinishStageIfReady();
	void CleanupStage();
	void UpdateTiming();
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
	TMap<FGameplayTag, TWeakObjectPtr<AActor>> Members;
	TArray<TWeakObjectPtr<UStageMemberComponent>> BoundMembers;
	UPROPERTY(Transient)
	TArray<FStageRootExecution> RootExecutions;
	TArray<float> PlannedStepStarts;
	TMap<FGameplayTag, EStageCommandResult> PreviousStepResults;
	TMap<FGameplayTag, EStageCommandResult> CurrentStepResults;
	TArray<int32> ReactionActivationCounts;
	TSet<TWeakObjectPtr<UStageMemberCommand>> AccountedCommands;
	FText FailureReason;
	double StageStartedAt = 0.0;
	int32 EventDispatchDepth = 0;
	bool bIssuingCommands = false;
	bool bAdvancing = false;
	bool bAdvanceRequested = false;
};
