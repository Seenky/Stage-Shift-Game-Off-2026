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
    bool ValidateDefinition(TArray<FString>& Errors, const FString& Path = TEXT("RecoveryRules")) const;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FStageActionRule> Rules;
	// Maximum wait when no recovery Action is available; zero means unlimited.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RecoveryWaitTimeout = 5.0f;
	
};
