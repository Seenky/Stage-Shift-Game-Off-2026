#include "UI/MenuCharacterDisplay.h"

#include "Animation/AnimSequence.h"
#include "Components/Customization/CharacterHairComponent.h"
#include "Core/StageShiftCharacter.h"

AMenuCharacterDisplay::AMenuCharacterDisplay()
{
	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
	SetRootComponent(BodyMesh);
	BodyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	OutfitMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("OutfitMesh"));
	HatMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HatMesh"));
	HairMesh = CreateDefaultSubobject<UCharacterHairComponent>(TEXT("HairMesh"));
	HairMesh->SetupAttachment(BodyMesh);
	for (USkeletalMeshComponent* Part : {OutfitMesh.Get(), HatMesh.Get()})
	{
		Part->SetupAttachment(BodyMesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AMenuCharacterDisplay::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	InitializeDisplay();
}

void AMenuCharacterDisplay::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	InitializeDisplay();
}

void AMenuCharacterDisplay::ApplyAppearance(const FStageShiftCharacterAppearance& Appearance)
{
	Appearance.ApplyTo(*BodyMesh, *OutfitMesh, *HatMesh, *HairMesh);
	HairMesh->SetHairPhysicsEnabled(false);
}

void AMenuCharacterDisplay::InitializeDisplay()
{
	if (const UClass* SourceClass = DefaultAppearanceSource.LoadSynchronous())
	{
		ApplyAppearance(GetDefault<AStageShiftCharacter>(SourceClass)->GetAppearance());
	}
	OutfitMesh->SetLeaderPoseComponent(BodyMesh);
	HatMesh->SetLeaderPoseComponent(BodyMesh);
	BodyMesh->SetUpdateAnimationInEditor(true);
	BodyMesh->OverrideAnimationData(PoseAnimation, false, false);
}
