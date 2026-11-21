// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StageData.h"
#include "Stage/Cues/StageCueTypes.h"
#include "StructUtils/PropertyBag.h"
#include "Engine/DataAsset.h"
#include "StageDataAsset.generated.h"

UCLASS()

class STAGESHIFT_API UStageDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing", meta = (MultiLine))
	FText SceneBriefing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Cues", meta = (TitleProperty = "CueId"))
	TArray<FStageCueDefinition> PlayerCues;

	UPROPERTY(EditAnywhere, Category = "Graph Parameters") FInstancedPropertyBag Parameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FName StageKey;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FName> RoleIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	TArray<FName> TargetIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced", meta = (ClampMin = "0.1", Units = "s"))
	float FinaleBudget = 60.0f;

	UFUNCTION(BlueprintPure, Category = "Scenario")

	FName GetEffectiveStageKey() const { return StageKey.IsNone() ? StageId.GetTagName() : StageKey; }
	UFUNCTION()
	TArray<FName> GetRoleOptions() const;
	UFUNCTION()

	TArray<FName> GetTargetOptions() const { return TargetIds; }
	UFUNCTION()
	TArray<FName> GetStepOptions() const;

	bool ValidateDefinition(TArray<FString>& Errors, const FInstancedPropertyBag* Overrides = nullptr) const;
	int32 FindStepIndex(FName StepId) const;
	bool GetStepRange(FName From, FName To, TSet<FName>& OutSteps) const;
	bool ValidateStepLinks(TArray<FString>& Errors) const;
#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
#if WITH_EDITORONLY_DATA
	// Layout only. EdGraph is transient and never serialized as a second scenario.
	UPROPERTY()
	TMap<FGuid, FVector2D> EditorPositions;
	UPROPERTY()
	TArray<FStageGraphGroup> EditorGroups;
	UPROPERTY() TArray<FStageGraphDraft> EditorDrafts;
	UPROPERTY() TMap<FGuid, FStageGraphViewState> EditorViews;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Advanced|Legacy")
	FGameplayTag StageId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, UIMin = 0.0, Units = "s"))
	float MainDuration = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	EStageTimingPolicy TimingPolicy = EStageTimingPolicy::CompleteOrDeadline;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, UIMin = 0.0, Units = "s"))
	float AdditiveDuration = 0.0f;
	/*Lateness - missing seconds*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, UIMin = 0.0))
	TMap<EStageLatenessLevel, float> LatenessThreshold = {
	    {EStageLatenessLevel::OnTime, 0},
	    {EStageLatenessLevel::Easy, 3},
	    {EStageLatenessLevel::Medium, 6},
	    {EStageLatenessLevel::Hard, 10},
	};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (GetActorFilter = "CanSelectParticipant"))
	TMap<FGameplayTag, TSoftObjectPtr<AActor>> StageMembers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (TitleProperty = "StepId"))
	TArray<FStageStep> Steps;
	UPROPERTY()
	bool bUseStepLinks = false;
	UPROPERTY()
	FName EntryStepId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions", meta = (TitleProperty = "ReactionId"))
	TArray<FStageMemberReaction> MemberReactions;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (TitleProperty = "RoleId"))
	TArray<FStageMemberAssignment> Finale;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 EstimatedScore = 0;

private:
	friend class AStageManager;

	UFUNCTION(BlueprintCallable, CallInEditor)
	void RecalculateEstimatedScore();

	UFUNCTION()
	bool CanSelectParticipant(const AActor* Actor) const;
};
