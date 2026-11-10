#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "StageShiftCharacterMovementComponent.generated.h"

UCLASS()
class STAGESHIFT_API UStageShiftCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;

protected:
	virtual void InitializeComponent() override;
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport) override;

private:
	TWeakObjectPtr<class UPhysicsManipulationComponent> Manipulation;
	bool SweepCharacterBody(const FVector& Delta, FHitResult& OutHit) const;
};
