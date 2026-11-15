#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageAction.h"
#include "WaitForStatusAction.generated.h"

class UStatusesComponent;

UCLASS()
class STAGESHIFT_API UWaitForStatusAction : public UStageAction
{
	GENERATED_BODY()

public:
	void SetupWaitForStatus(UStatusesComponent* InTargetStatuses, const FGameplayTag& InStatusTag);

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;

private:
	UFUNCTION()
	void HandleStatusesRemoved(FGameplayTagContainer RemovedStatuses);

	TWeakObjectPtr<UStatusesComponent> TargetStatuses;
	FGameplayTag StatusTag;
};
