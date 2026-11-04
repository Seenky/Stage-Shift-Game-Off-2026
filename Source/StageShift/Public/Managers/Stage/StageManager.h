// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageData.h"
#include "Components/StateTreeComponent.h"
#include "StageManager.generated.h"


UCLASS()
class STAGESHIFT_API AStageManager : public AActor
{
	GENERATED_BODY()

public:
	AStageManager();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(NoEditInline))
	TObjectPtr<UStateTreeComponent> StateTreeComponent;

	TArray<FStageData> GetStages() const { return Stages; }
	
private:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage Shift", meta=(AllowPrivateAccess))
	TArray<FStageData> Stages;
};