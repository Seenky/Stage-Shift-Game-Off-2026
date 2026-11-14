// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageAction.h"
#include "StageLookToAction.generated.h"

UCLASS()
class STAGESHIFT_API UStageLookToAction : public UStageAction
{
	GENERATED_BODY()
	
public:
	
	void SetupLookTo(
		const FVector& InLookToLoc, 
		const float& InTurnSpeed,
		const float& InAcceptanceAngleDeg);

protected:
	
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;
	virtual EStageActionStopMode GetStopMode_Implementation(EStageActionStopReason Reason) const override;
	
private:
	
	bool GetIsSucceeded(const FVector& InLookToLoc) const;
	void TurnAtLoc();
	
	FTimerHandle LookTimerHandle;
	FVector LookToLoc;
	float TurnSpeed; // Angle in second
	float AcceptanceDotThreshold; // Angle in second
};
