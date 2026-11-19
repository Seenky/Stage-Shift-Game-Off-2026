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
	TArray<FName> TargetIds;

	// Serialized by older assets and may still be set by a graph parameter binding.
	UPROPERTY(BlueprintReadWrite, Category = "Scenario")
	FName TargetId;

	virtual void PostLoad() override;
	TConstArrayView<FName> GetMoveTargets() const;

	virtual uint8 GetResourceMask() const override { return 3; }

	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot, float InPlannedStartTime, float InPlannedEndTime) override;

protected:
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual float GetNeededTime_Implementation() const override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
	virtual void OnPreStart_Implementation(const FStageExecutionContext& InContext) override;
	virtual void OnStart_Implementation() override;

private:
	AActor* ResolveMoveTarget(const FStageExecutionContext& InContext);

	UFUNCTION()
	void HandleCommandFinished(UStageMemberCommand* Command, EStageCommandResult Result);

	UPROPERTY(Category = "Advanced", AdvancedDisplay, EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess))
	float AcceptanceRadius = 10.f;

	FName SelectedTargetId;
	float NeedMoveTime = 0.0f;
	TWeakObjectPtr<AAIController> AIController;
};
