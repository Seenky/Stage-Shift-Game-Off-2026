#pragma once

#include "Animation/AnimInstance.h"
#include "Components/Customization/CharacterHairStyle.h"
#include "StageShiftHairAnimInstance.generated.h"

UCLASS(Blueprintable)
class STAGESHIFT_API UStageShiftHairAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Hair")
	FCharacterHairPhysicsSettings PhysicsSettings;
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Hair")
	TArray<FCharacterHairStrand> Strands;
};
