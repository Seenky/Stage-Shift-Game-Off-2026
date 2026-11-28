#include "Components/Customization/StageShiftCharacterAppearance.h"

#include "Components/Customization/CharacterHairComponent.h"
#include "Components/Customization/CharacterHairStyle.h"
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
	if (bBodyChanged || HairMesh.Hairstyle != Hairstyle)
	{
		HairMesh.SetHairstyle(Hairstyle);
	}
}
