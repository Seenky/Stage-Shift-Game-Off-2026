#pragma once

#include "RigUnit_PBIK.h"
#include "StageShiftPickup.generated.h"

USTRUCT(meta=(DisplayName="Pickup Reach", Category="Stage Shift"))
struct FRigUnit_StageShiftPickup : public FRigUnitMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	TArray<FPBIKBoneSetting> BoneSettings;

private:
	UPROPERTY(Transient)
	FRigUnit_PBIK Solver;
};
