#pragma once

#include "Engine/EngineTypes.h"
#include "Units/RigUnit.h"
#include "StageShiftFootPlacement.generated.h"

USTRUCT()
struct FStageShiftFootPlacementState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector PlantLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector PlantOffset = FVector::ZeroVector;

	UPROPERTY()
	FVector Normal = FVector::UpVector;

	UPROPERTY()
	double Height = 0.0;

	UPROPERTY()
	double HeightVelocity = 0.0;

	UPROPERTY()
	float Weight = 0.0f;

	UPROPERTY()
	float PinAlpha = 0.0f;

	UPROPERTY()
	bool bPlanted = false;
};

USTRUCT(meta = (DisplayName = "Stage Shift Foot Placement", Category = "Stage Shift"))
struct FRigUnit_StageShiftFootPlacement : public FRigUnitMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(meta = (Input, ClampMin = "0.0", ClampMax = "89.0"))
	float WalkableFloorAngle = 50.0f;

	UPROPERTY(meta = (Output))
	FTransform LeftTarget = FTransform::Identity;

	UPROPERTY(meta = (Output))
	FTransform RightTarget = FTransform::Identity;

	UPROPERTY(meta = (Output))
	float LeftWeight = 0.0f;

	UPROPERTY(meta = (Output))
	float RightWeight = 0.0f;

	UPROPERTY(meta = (Output))
	float PelvisOffset = 0.0f;

private:
	UPROPERTY(meta = (Transient))
	FStageShiftFootPlacementState LeftState;

	UPROPERTY(meta = (Transient))
	FStageShiftFootPlacementState RightState;

	UPROPERTY(meta = (Transient))
	FVector PreviousRoot = FVector::ZeroVector;

	UPROPERTY(meta = (Transient))
	double PelvisHeight = 0.0;

	UPROPERTY(meta = (Transient))
	double PelvisVelocity = 0.0;

	UPROPERTY(meta = (Transient))
	bool bHasPreviousRoot = false;
};
