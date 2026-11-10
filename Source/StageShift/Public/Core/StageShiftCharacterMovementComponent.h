#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "StageShiftCharacterMovementComponent.generated.h"

UCLASS()
class STAGESHIFT_API UStageShiftCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected:
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport) override;

private:
	bool SweepCharacterBody(const FVector& Delta, FHitResult& OutHit) const;
};
