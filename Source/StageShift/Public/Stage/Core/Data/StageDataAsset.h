// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StageData.h"
#include "Engine/DataAsset.h"
#include "StageDataAsset.generated.h"


UCLASS()
class STAGESHIFT_API UStageDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag StageId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, UIMin = 0.0))
	float MainDuration;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, UIMin = 0.0))
	float AdditiveDuration;
	/*Lateness - missing seconds*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0, UIMin = 0.0))
	TMap<EStageLatenessLevel, float> LatenessThreshold = {
		{EStageLatenessLevel::OnTime, 0},
		{EStageLatenessLevel::Easy, 3},
		{EStageLatenessLevel::Medium, 6},
		{EStageLatenessLevel::Hard, 10},
	};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(GetActorFilter = "CanSelectParticipant"))
	TMap<FGameplayTag, TSoftObjectPtr<AActor>> StageMembers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FStageStep> Steps;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FStageMemberReaction> MemberReactions;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
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
