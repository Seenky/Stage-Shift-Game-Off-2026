#pragma once

#include "CoreMinimal.h"
#include "Components/Interaction/InteractionTypes.h"
#include "Stage/Core/StageMemberCommand.h"
#include "InteractCommand.generated.h"

UCLASS(meta = (StageCategory = "Interaction"))
class STAGESHIFT_API UInteractCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	UInteractCommand();
	virtual uint8 GetResourceMask() const override { return 3; }
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual float GetNeededTime_Implementation() const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess, GetOptions = "GetTargetOptions"))
	FName TargetId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess))
	FGameplayTag ActionTag;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Advanced", meta = (AllowPrivateAccess, ToolTip = "Start an interaction, or send End to release a previously latched interaction."))
	EInteractionPhase InteractionPhase = EInteractionPhase::Start;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Advanced", meta = (AllowPrivateAccess, ClampMin = "0.0", Units = "s", ToolTip = "Start + Active: wait this long before sending End. End: pass this duration immediately as throw charge; 0 drops the carried item. Throw starts above PhysicsManipulationComponent.TapDuration. Completed results finish immediately."))
	float HoldDuration = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "Advanced", meta = (AllowPrivateAccess, ClampMin = "1.0", Units = "cm"))
	float MaxDistance = 150.0f;
};
