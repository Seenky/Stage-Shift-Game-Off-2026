// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "Stage/Core/StageAction.h"
#include "StageMoveAction.generated.h"

class AAIController;

UCLASS()

class STAGESHIFT_API UStageMoveAction : public UStageAction
{
	GENERATED_BODY()

public:
	void MoveToRequest(const FAIMoveRequest& InRequest, float InTimeLimit = 0.0f);

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;

private:
	void LogMove(const FString& Details) const;
	bool FailIfTargetHeld();

	UFUNCTION()
	void HandleReceiveMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	FAIRequestID CurrentRequestID;
	FAIMoveRequest MoveRequest;
	float TimeLimit = 0.0f;
	TWeakObjectPtr<AAIController> AIController;
	FTimerHandle TargetAvailabilityTimer;
	FTimerHandle MoveTimeoutTimer;
	bool bStartingRequest = false;
	TArray<TPair<FAIRequestID, EPathFollowingResult::Type>> StartingCompletions;
};
