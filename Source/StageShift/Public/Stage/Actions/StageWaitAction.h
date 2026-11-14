// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageAction.h"
#include "StageWaitAction.generated.h"


UCLASS()
class STAGESHIFT_API UStageWaitAction : public UStageAction
{
	GENERATED_BODY()
	
public:
	
	void SetupWait(const float& InTime);
	float GetWaitTime() const { return WaitTime; }
	
protected:
	
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;
	
private:
	
	void FinishTimer();
	
	float WaitTime;
	FTimerHandle WaitTimerHandle;
};
