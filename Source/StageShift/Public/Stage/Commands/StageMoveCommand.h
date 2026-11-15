// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StageMoveCommand.generated.h"

class AAIController;

UCLASS()

class STAGESHIFT_API UStageMoveCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (GetOptions = "GetTargetOptions"))
	FName TargetId;
	AActor* ResolveTarget(const FStageExecutionContext& InContext) const;

	virtual uint8 GetResourceMask() const override { return 3; }

	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot, float InPlannedStartTime, float InPlannedEndTime) override;

protected:
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual void OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult) override;
	virtual float GetNeededTime_Implementation() const override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
	virtual void OnPreStart_Implementation(const FStageExecutionContext& InContext) override;
	virtual void OnStart_Implementation() override;

private:
	UFUNCTION()
	void HandleCommandFinished(UStageMemberCommand* Command, EStageCommandResult Result);

	UPROPERTY(Category = "Advanced", AdvancedDisplay, EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess))
	float AcceptanceRadius = 10.f;

	float NeedMoveTime;
	FAIRequestID AIRequestID;
	TWeakObjectPtr<AAIController> AIController;
};
