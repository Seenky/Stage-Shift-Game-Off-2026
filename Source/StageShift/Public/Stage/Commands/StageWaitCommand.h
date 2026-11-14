// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StageWaitCommand.generated.h"


UCLASS()
class STAGESHIFT_API UStageWaitCommand : public UStageMemberCommand
{
	GENERATED_BODY()
	
public:
	virtual float GetNeededTime_Implementation() const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual void OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult) override;
	virtual void OnPreStart_Implementation(const FStageExecutionContext& InContext) override;
private:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess, ClampMin=0.0001, UIMin=0.0001))
	float WaitTime = 3.f;
	
	float CurrentTime = 0.f;
};