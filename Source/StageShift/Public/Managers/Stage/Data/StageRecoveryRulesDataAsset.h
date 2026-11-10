// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageData.h"
#include "Engine/DataAsset.h"
#include "StageRecoveryRulesDataAsset.generated.h"


UCLASS()
class STAGESHIFT_API UStageRecoveryRulesDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FStageActionRule> Rules;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float RecoveryWaitTimeout = 5.0f;
	
};
