// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StageLookToCommand.generated.h"

UCLASS()
class STAGESHIFT_API UStageLookToCommand : public UStageMemberCommand
{
	GENERATED_BODY()
	
public:
	UStageLookToCommand();

	virtual void ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot,
		float InPlannedStartTime, float InPlannedEndTime) override;

	
	virtual float GetNeededTime_Implementation() const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
	
private:
	UFUNCTION()
	void HandleCommandFinished(UStageMemberCommand* Command, EStageCommandResult Result);

	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
	TSoftObjectPtr<AActor> LookToActor;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
	float TurnSpeed = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
	float AcceptanceAngleDeg = 5.f;
};
