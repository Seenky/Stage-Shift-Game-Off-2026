#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Stage/Core/StageAction.h"
#include "TimerManager.h"
#include "PickUpAction.generated.h"

class UAnimInstance;
class UAnimMontage;
class UPhysicsManipulationComponent;
struct FInteractionContext;

UCLASS()
class STAGESHIFT_API UPickUpAction : public UStageAction
{
	GENERATED_BODY()

public:
	void SetupPickUp(TArray<TWeakObjectPtr<AActor>> InCandidates, float InMaxDistance, TOptional<TRange<double>> InWindow,
		float InCheckDistanceTime, float InPickUpPeriodTime);

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;

private:
	UFUNCTION()
	void HandleItemPickedUp(AActor* Item);
	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void CheckForItem();
	bool FindPickup(FInteractionContext& OutInteraction) const;
	bool BuildPickupContext(AActor* Item, FInteractionContext& OutInteraction, FString* OutFailure = nullptr) const;
	void StartPickup(const FInteractionContext& Interaction);
	void FailPickup(const FString& Details);
	void CancelPickup();
	void StopPickupMontage();

	TArray<TWeakObjectPtr<AActor>> Candidates;
	TOptional<TRange<double>> PickupWindow;
	FTimerHandle CheckTimer;
	double NextPickupAttemptAt = 0.0;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<UPhysicsManipulationComponent> Manipulator;
	TWeakObjectPtr<UAnimInstance> PlaybackInstance;
	TWeakObjectPtr<UAnimMontage> PlayingMontage;
	FHitResult InteractionHit;
	float MaxDistance = 150.0f;
	float CheckDistanceTime = 0.1f;
	float PickUpPeriodTime = 0.2f;
	int32 MontageInstanceId = INDEX_NONE;
	bool bStarting = false;
	bool bOwnPickup = false;
};
