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
	void MoveToRequest(const FVector& InMoveToLoc, const float& InAcceptanceRadius, const FAIRequestID& LastRequestID, bool bInIncludeAgentRadius = true);

	FAIRequestID GetCurrentRequestID() const { return CurrentRequestID; }

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;

private:
	void LogMove(const FString& Details) const;

	UFUNCTION()
	void HandleReceiveMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	FAIRequestID CurrentRequestID;
	FVector MoveToLoc;
	float AcceptanceRadius;
	bool bIncludeAgentRadius = true;
	TWeakObjectPtr<AAIController> AIController;
	bool bStartingRequest = false;
	TArray<TPair<FAIRequestID, EPathFollowingResult::Type>> StartingCompletions;
};
