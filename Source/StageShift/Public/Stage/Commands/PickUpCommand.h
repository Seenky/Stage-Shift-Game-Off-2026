#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageMemberCommand.h"
#include "PickUpCommand.generated.h"

UCLASS(meta = (StageCategory = "Interaction"))
class STAGESHIFT_API UPickUpCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	UPickUpCommand();
	virtual void PostLoad() override;
	virtual uint8 GetResourceMask() const override { return 3; }
	virtual bool ExpectsStageProp() const override { return true; }
	virtual bool AcceptsStageProp(const AActor& Prop) const override;
	virtual TOptional<TRange<double>> GetPropThrowWindow() const override { return PropThrowWindow; }
	float GetMaxDistance() const { return MaxDistance; }
	TConstArrayView<FName> GetPickupTargets() const;
	void PlayWrongPropLine();
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnPreStart_Implementation(const FStageExecutionContext& InContext) override;
	virtual float GetNeededTime_Implementation() const override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess, GetOptions = "GetTargetOptions"))
	TArray<FName> TargetIds;
	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (MultiLine, ToolTip = "Optional line when a wrong prop reaches the actor during the catch window."))
	FText WrongPropLine;

	// Serialized by older assets and may still be set by a graph parameter binding.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess))
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Advanced",
	    meta = (AllowPrivateAccess, ClampMin = "1.0", Units = "cm", ToolTip = "Maximum distance for picking up a prop from the ground or catching it in flight."))
	float MaxDistance = 150.0f;

	UPROPERTY(EditAnywhere, Category = "Throw Window", meta = (ClampMin = "0.0", Units = "s"))
	float ThrowWindowLead = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Throw Window", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Time to accept throws and wait for nearby props after the lead. Zero uses a single immediate pickup attempt."))
	float ThrowWindowDuration = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Throw Window", meta = (ClampMin = "0.01", Units = "s", ToolTip = "Interval between checks for an available prop during the window."))
	float CheckDistanceTime = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Throw Window", meta = (ClampMin = "0.01", Units = "s", ToolTip = "Minimum delay after an unsuccessful pickup attempt during the window."))
	float PickUpPeriodTime = 0.2f;

	TOptional<TRange<double>> PropThrowWindow;
	float NeededTime = 0.0f;
	bool bWrongPropLineTriggered = false;
};
