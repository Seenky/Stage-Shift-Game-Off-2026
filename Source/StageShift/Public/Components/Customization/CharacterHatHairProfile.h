#pragma once

#include "Engine/AssetUserData.h"
#include "CharacterHatHairProfile.generated.h"

UCLASS(BlueprintType, EditInlineNew)
class STAGESHIFT_API UCharacterHatHairProfile : public UAssetUserData
{
	GENERATED_BODY()

public:
	// Hats with the same inner shape and brim line share this morph name.
	UPROPERTY(EditAnywhere, Category = "Hair Fit")
	FName MorphTarget;
};
