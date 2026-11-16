#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Stage/Core/StageAction.h"
#include "PickUpAction.generated.h"

class UAnimInstance;
class UAnimMontage;
class UPhysicsManipulationComponent;

UCLASS()
class STAGESHIFT_API UPickUpAction : public UStageAction
{
	GENERATED_BODY()

public:
	void SetupPickUp(AActor* Item, float InMaxDistance);

protected:
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;

private:
	UFUNCTION()
	void HandleItemPickedUp(AActor* Item);
	UFUNCTION()
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void CancelPickup();
	void StopPickupMontage();

	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<UPhysicsManipulationComponent> Manipulator;
	TWeakObjectPtr<UAnimInstance> PlaybackInstance;
	TWeakObjectPtr<UAnimMontage> PlayingMontage;
	FHitResult InteractionHit;
	float MaxDistance = 150.0f;
	int32 MontageInstanceId = INDEX_NONE;
	bool bStarting = false;
	bool bOwnPickup = false;
};
