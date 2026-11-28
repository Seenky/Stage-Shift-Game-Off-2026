#pragma once

#include "Components/Customization/StageShiftCharacterAppearance.h"
#include "GameFramework/Actor.h"
#include "MenuCharacterDisplay.generated.h"

class AStageShiftCharacter;
class UAnimSequence;

UCLASS()
class STAGESHIFT_API AMenuCharacterDisplay : public AActor
{
	GENERATED_BODY()

public:
	AMenuCharacterDisplay();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostRegisterAllComponents() override;

	UFUNCTION(BlueprintCallable, Category = "Character|Appearance")
	void ApplyAppearance(const FStageShiftCharacterAppearance& Appearance);

private:
	void InitializeDisplay();

	UPROPERTY(VisibleAnywhere, Category = "Character|Meshes")
	TObjectPtr<USkeletalMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category = "Character|Meshes")
	TObjectPtr<USkeletalMeshComponent> OutfitMesh;

	UPROPERTY(VisibleAnywhere, Category = "Character|Meshes")
	TObjectPtr<USkeletalMeshComponent> HatMesh;

	UPROPERTY(VisibleAnywhere, Category = "Character|Meshes")
	TObjectPtr<UCharacterHairComponent> HairMesh;

	UPROPERTY(EditAnywhere, Category = "Character|Appearance")
	TSoftClassPtr<AStageShiftCharacter> DefaultAppearanceSource;

	UPROPERTY(EditAnywhere, Category = "Character|Pose")
	TObjectPtr<UAnimSequence> PoseAnimation;
};
