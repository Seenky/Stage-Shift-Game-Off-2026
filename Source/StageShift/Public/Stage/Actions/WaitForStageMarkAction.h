#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Stage/Core/StageAction.h"
#include "WaitForStageMarkAction.generated.h"

class UStageMemberComponent;

UCLASS()
class STAGESHIFT_API UWaitForStageMarkAction : public UStageAction
{
	GENERATED_BODY()

public:
	void SetupWaitForStageMark(UStageMemberComponent* InTargetMember);

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;

private:
	void CheckTarget();
	void HandleWorldTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);

	TWeakObjectPtr<UStageMemberComponent> TargetMember;
	FDelegateHandle WaitTickHandle;
};
