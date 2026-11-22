#pragma once

#include "Stage/Core/StageAction.h"
#include "Stage/Environment/StageSpotlightControl.h"
#include "StageSpotlightAction.generated.h"

UCLASS(NotBlueprintable)
class STAGESHIFT_API UStageSpotlightAction : public UStageAction
{
	GENERATED_BODY()

public:
	void Setup(AStageSpotlightControl* InSpotlight, AActor* InFocusActor, FName InTargetRoleId, const FText& InTargetName, float InTimeLimit,
	    FName InCueId);

protected:
	virtual void OnStart_Implementation() override;
	virtual void Cleanup_Implementation() override;

private:
	UFUNCTION()
	void HandleSpotlightChanged(FName ChangedCueId, AActor* Target, EStageSpotlightState State);

	UFUNCTION()
	void HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason);

	void HandleTimeout();

	TWeakObjectPtr<AStageSpotlightControl> Spotlight;
	TWeakObjectPtr<AActor> FocusActor;
	FName TargetRoleId;
	FText TargetName;
	FName CueId;
	FTimerHandle Timeout;
	float TimeLimit = 0.0f;
	double Deadline = 0.0;
	bool bCueShown = false;
};
