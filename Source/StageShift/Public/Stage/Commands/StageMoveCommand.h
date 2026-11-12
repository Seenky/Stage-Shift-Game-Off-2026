// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StageMoveCommand.generated.h"

UCLASS()
class STAGESHIFT_API UStageMoveCommand : public UStageMemberCommand
{
	GENERATED_BODY()
	
protected:
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual void OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult) override;
	const float GetNeedMoveToTime() const;
	virtual float GetNeededTime_Implementation() const override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
private:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
	TSoftObjectPtr<AActor> MoveToActor;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
	float AcceptableRadius = 10.f;
	
	FAIRequestID AIRequestID;
};
