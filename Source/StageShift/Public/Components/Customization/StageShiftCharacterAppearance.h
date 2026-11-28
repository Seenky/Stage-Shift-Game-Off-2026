#pragma once

#include "CoreMinimal.h"
#include "StageShiftCharacterAppearance.generated.h"

class UCharacterHairComponent;
class UCharacterHairStyle;
class USkeletalMesh;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct STAGESHIFT_API FStageShiftCharacterAppearance
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<USkeletalMesh> Body;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<USkeletalMesh> Outfit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<USkeletalMesh> Hat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UCharacterHairStyle> Hairstyle;

	void ApplyTo(USkeletalMeshComponent& BodyMesh, USkeletalMeshComponent& OutfitMesh, USkeletalMeshComponent& HatMesh, UCharacterHairComponent& HairMesh) const;
};
