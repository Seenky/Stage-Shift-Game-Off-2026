// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageAction.h"
#include "ReturnToStageMarkAction.generated.h"


UCLASS()
class STAGESHIFT_API UReturnToStageMarkAction : public UStageAction
{
	GENERATED_BODY()
	
	
private:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
	float TurnSpeed = 180.f;
};
