#include "Animation/StageShiftHairAnimInstance.h"

#include "Components/Customization/CharacterHairComponent.h"

void UStageShiftHairAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Strands.Reset();
	PhysicsSettings = FCharacterHairPhysicsSettings();
	const UCharacterHairComponent* Hair = Cast<UCharacterHairComponent>(GetSkelMeshComponent());
	if (Hair && Hair->Hairstyle && Hair->IsHairPhysicsEnabled())
	{
		PhysicsSettings = Hair->Hairstyle->Physics;
		Strands = Hair->Hairstyle->Strands;
	}
	else
	{
		PhysicsSettings.Alpha = 0.0f;
	}
}
