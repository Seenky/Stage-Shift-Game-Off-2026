#pragma once

#include "Engine/DataAsset.h"
#include "StageShiftCharacterPreset.generated.h"

class UCharacterHairStyle;
class USkeletalMesh;

UCLASS(BlueprintType)
class STAGESHIFT_API UStageShiftCharacterPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<UCharacterHairStyle> Hairstyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<USkeletalMesh> Hat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TObjectPtr<USkeletalMesh> Outfit;
};
