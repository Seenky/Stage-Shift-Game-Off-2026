#include "Components/Customization/StageShiftCharacterAppearance.h"

#include "Components/Customization/CharacterHairComponent.h"
#include "Components/Customization/CharacterHatHairProfile.h"
#include "Engine/SkeletalMesh.h"

void FStageShiftCharacterAppearance::ApplyTo(
    USkeletalMeshComponent& BodyMesh, USkeletalMeshComponent& OutfitMesh, USkeletalMeshComponent& HatMesh, UCharacterHairComponent& HairMesh) const
{
	const bool bBodyChanged = Body && BodyMesh.GetSkeletalMeshAsset() != Body;
	if (bBodyChanged)
	{
		BodyMesh.SetSkeletalMesh(Body);
	}
	OutfitMesh.SetSkeletalMesh(Outfit);
	HatMesh.SetSkeletalMesh(Hat);
	const UCharacterHatHairProfile* HatProfile = Hat ? Cast<UCharacterHatHairProfile>(Hat->GetAssetUserDataOfClass(UCharacterHatHairProfile::StaticClass())) : nullptr;
	const FName HatFitMorph = HatProfile ? HatProfile->MorphTarget : NAME_None;
	if (bBodyChanged || HairMesh.Hairstyle != Hairstyle || HairMesh.GetHatFitMorph() != HatFitMorph)
	{
		HairMesh.SetHairstyleForHat(Hairstyle, HatFitMorph);
	}
}
