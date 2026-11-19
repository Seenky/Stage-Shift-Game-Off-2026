#pragma once

#include "CoreMinimal.h"
#include "Components/Interaction/InteractionTypes.h"
#include "Stage/Core/StageAction.h"
#include "InteractAction.generated.h"

UCLASS()
class STAGESHIFT_API UInteractAction : public UStageAction
{
	GENERATED_BODY()

public:
	void SetupInteraction(AActor* TargetActor, FGameplayTag InActionTag, EInteractionPhase InPhase, float InHoldDuration, float InMaxDistance);

protected:
	virtual void OnStart_Implementation() override;
	virtual void Cleanup_Implementation() override;

private:
	void EndInteraction();
	EInteractionResult SendEnd(bool bCancelled);

	TWeakObjectPtr<AActor> Target;
	FGameplayTag ActionTag;
	EInteractionPhase InteractionPhase = EInteractionPhase::Start;
	FHitResult InteractionHit;
	float HoldDuration = 0.0f;
	float MaxDistance = 150.0f;
	double StartedAt = 0.0;
	FTimerHandle HoldTimer;
	bool bInteractionActive = false;
};
