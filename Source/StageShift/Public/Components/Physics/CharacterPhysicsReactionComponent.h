#pragma once

#include "Components/ActorComponent.h"
#include "Animation/CharacterPhysicsAnimationSet.h"
#include "CharacterPhysicsReactionComponent.generated.h"

/** Selects character reactions from data and delegates their execution to the montage component. */
UCLASS(ClassGroup = Physics, meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UCharacterPhysicsReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Physics Reaction")
	bool PlayReaction(ECharacterPhysicsReaction Reaction, const FVector& WorldDirection, const FPhysicalAnimationInput& Input);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics Reaction")
	TObjectPtr<UCharacterPhysicsAnimationSet> AnimationSet;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	const UCharacterPhysicsAnimationSet* GetAnimationSet() const;
	void CaptureRecoveryFrame();
	FPhysicsRecoverySelection SelectRecovery(const USkeletalMeshComponent* Mesh) const;

	TWeakObjectPtr<UPhysicalMontageComponent> Executor;
	FDelegateHandle StartedHandle;
	FVector LocalFacingDirection = FVector::ForwardVector;
	bool bHasRecoveryFrame = false;
};
