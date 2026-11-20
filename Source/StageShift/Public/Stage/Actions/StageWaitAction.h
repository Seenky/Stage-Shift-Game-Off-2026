// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageAction.h"
#include "StageWaitAction.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;

UCLASS()

class STAGESHIFT_API UStageWaitAction : public UStageAction
{
	GENERATED_BODY()

public:
	void SetupWait(float InTime, UAnimSequenceBase* InGesture = nullptr);

	float GetWaitTime() const { return WaitTime; }

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;

private:
	void FinishTimer();

	float WaitTime = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Gesture;
	TWeakObjectPtr<UAnimInstance> GestureInstance;
	TWeakObjectPtr<UAnimMontage> GestureMontage;
	FTimerHandle WaitTimerHandle;
};
