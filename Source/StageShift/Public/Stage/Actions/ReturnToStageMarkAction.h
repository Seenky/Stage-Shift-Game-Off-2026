// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageAction.h"
#include "ReturnToStageMarkAction.generated.h"

UCLASS()

class STAGESHIFT_API UReturnToStageMarkAction : public UStageAction
{
	GENERATED_BODY()

public:
	virtual void ValidateRecoveryTemplate(const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual void OnStart_Implementation() override;
	virtual void Cleanup_Implementation() override;

private:
	void BeginTurn();
	UFUNCTION()
	void HandleChildFinished(UStageAction* Action, EStageActionResult Result);
	UPROPERTY(Transient)
	TObjectPtr<class UStageMoveAction> MoveAction;
	UPROPERTY(Transient)
	TObjectPtr<class UStageLookToAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess))
	float TurnSpeed = 180.f;
};
