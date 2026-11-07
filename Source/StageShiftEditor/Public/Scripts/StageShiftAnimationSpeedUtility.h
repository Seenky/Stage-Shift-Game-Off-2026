#pragma once

#include "AssetActionUtility.h"
#include "StageShiftAnimationSpeedUtility.generated.h"

class UAnimSequence;

UCLASS()
class STAGESHIFTEDITOR_API UStageShiftAnimationSpeedUtility : public UAssetActionUtility
{
	GENERATED_BODY()

public:
	UStageShiftAnimationSpeedUtility();

	UFUNCTION(CallInEditor, Category = "Stage Shift|Animation")
	void BakeSpeedData();

	UFUNCTION(BlueprintCallable, Category = "Stage Shift|Animation")
	static bool BakeSequence(UAnimSequence* Animation);

	UFUNCTION(BlueprintCallable, Category = "Stage Shift|Animation")
	static bool BakeFootPlant(UAnimSequence* Animation);
};
