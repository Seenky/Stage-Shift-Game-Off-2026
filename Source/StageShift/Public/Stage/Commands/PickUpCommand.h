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
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual void OnPreStart_Implementation(const FStageExecutionContext& InContext) override;
	virtual float GetNeededTime_Implementation() const override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;

private:
	TConstArrayView<FName> GetPickupTargets() const;
	AActor* FindPickupTarget(const FStageExecutionContext& InContext) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess, GetOptions = "GetTargetOptions"))
	TArray<FName> TargetIds;

	// Serialized by older assets and may still be set by a graph parameter binding.
	UPROPERTY(BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess))
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Advanced", meta = (AllowPrivateAccess, ClampMin = "1.0", Units = "cm"))
	float MaxDistance = 150.0f;
	float NeededTime = 0.0f;
};
