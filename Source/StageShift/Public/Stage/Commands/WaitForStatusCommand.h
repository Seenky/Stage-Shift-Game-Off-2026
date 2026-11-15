#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageMemberCommand.h"
#include "WaitForStatusCommand.generated.h"

UCLASS()
class STAGESHIFT_API UWaitForStatusCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess, GetOptions = "GetRoleOptions"))
	FName TargetRoleId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess, ToolTip = "Wait until the selected role no longer has this status, including child tags."))
	FGameplayTag StatusTag;
};
