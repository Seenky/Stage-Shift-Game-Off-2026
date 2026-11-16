#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageMemberCommand.h"
#include "WaitForStageMarkCommand.generated.h"

UCLASS()
class STAGESHIFT_API UWaitForStageMarkCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	FName GetTargetRoleId() const { return TargetRoleId; }
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;

protected:
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scenario", meta = (AllowPrivateAccess, GetOptions = "GetRoleOptions"))
	FName TargetRoleId;
};
