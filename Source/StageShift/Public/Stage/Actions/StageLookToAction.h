// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Stage/Core/StageAction.h"
#include "StageLookToAction.generated.h"

UCLASS()

class STAGESHIFT_API UStageLookToAction : public UStageAction
{
	GENERATED_BODY()

public:
	void SetupLookTo(const FVector& InLookTarget, const float& InTurnSpeed, const float& InAcceptanceAngleDeg, bool bInUseTargetDirection = false);

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;
	virtual EStageActionStopMode GetStopMode_Implementation(EStageActionStopReason Reason) const override;

private:
	FVector GetLookDirection(const AActor& Owner) const;
	bool IsFacingTarget(const AActor& Owner) const;
	void TurnAtLoc(UWorld* World, ELevelTick TickType, float DeltaSeconds);

	FDelegateHandle LookTickHandle;
	FVector LookTarget;
	bool bUseTargetDirection = false;
	float TurnSpeed; // Angle in second
	float AcceptanceDotThreshold; // Angle in second
};
