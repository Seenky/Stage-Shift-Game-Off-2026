#pragma once

#include "Engine/DataAsset.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "CharacterPhysicsAnimationSet.generated.h"

UENUM(BlueprintType)
enum class ECharacterPhysicsReaction : uint8
{
	Impact,
	Stumble,
	Slip
};

UENUM(BlueprintType)
enum class ECharacterFallDirection : uint8
{
	Forward,
	Backward,
	Left,
	Right
};

UENUM(BlueprintType)
enum class ECharacterRecoveryPose : uint8
{
	Prone,
	Supine
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FCharacterPhysicsReactionAnimation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FPhysicalMontageSettings DefaultAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TMap<ECharacterFallDirection, FPhysicalMontageSettings> DirectionalAnimations;
};

/** Character-specific reaction choices; playback and physics remain executor responsibilities. */
UCLASS(BlueprintType)
class STAGESHIFT_API UCharacterPhysicsAnimationSet : public UDataAsset
{
	GENERATED_BODY()

public:
	UCharacterPhysicsAnimationSet();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions")
	TMap<ECharacterPhysicsReaction, FCharacterPhysicsReactionAnimation> Reactions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reactions")
	FPhysicalMontageSettings FallbackAnimation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	TMap<ECharacterRecoveryPose, TObjectPtr<UAnimMontage>> RecoveryAnimations;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	FName OrientationBone = TEXT("pelvis");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recovery")
	FName HeadingBone = TEXT("spine_03");
};
