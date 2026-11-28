#pragma once

#include "Components/SkeletalMeshComponent.h"
#include "CharacterHairComponent.generated.h"

class UCharacterHairStyle;

UCLASS(ClassGroup = (Customization), meta = (BlueprintSpawnableComponent))

class STAGESHIFT_API UCharacterHairComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	UCharacterHairComponent();
	virtual void OnRegister() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TObjectPtr<UCharacterHairStyle> Hairstyle;

	UFUNCTION(BlueprintCallable, Category = "Hair")
	void SetHairstyle(UCharacterHairStyle* NewHairstyle);

	void SetHairstyleForHat(UCharacterHairStyle* NewHairstyle, FName MorphTarget);
	FName GetHatFitMorph() const { return HatFitMorph; }
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Hair")
	void RefreshHairstyle();
	UFUNCTION(BlueprintCallable, Category = "Hair")
	void SetHairPhysicsEnabled(bool bEnabled);
	UFUNCTION(BlueprintPure, Category = "Hair")

	bool IsHairPhysicsEnabled() const { return bHairPhysicsEnabled; }
	UFUNCTION(BlueprintCallable, Category = "Hair")
	void ResetHairSimulation();

private:
	void ApplyHatFitMorph();

	FName HatFitMorph;
	bool bHairPhysicsEnabled = false;
};
