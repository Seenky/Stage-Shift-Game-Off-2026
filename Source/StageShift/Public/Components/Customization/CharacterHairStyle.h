#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CharacterHairStyle.generated.h"

class USkeletalMesh;
class UStageShiftHairAnimInstance;

UENUM(BlueprintType)
enum class ECharacterHairBoneAxis : uint8
{
	X, Y, Z
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FCharacterHairStrand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName BoundBone;
	// Leave empty for a single body. Otherwise this must descend from BoundBone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ChainEnd;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECharacterHairBoneAxis BoneAxis = ECharacterHairBoneAxis::Y;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0", Units = "cm"))
	FVector BoxExtents = FVector(1.0, 3.0, 1.0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Units = "cm"))
	FVector LocalJointOffset = FVector(0.0, 1.5, 0.0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	float SwingLimit = 15.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "45.0", Units = "deg"))
	float TwistLimit = 3.0f;
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FCharacterHairPhysicsSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Alpha = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.7", ClampMax = "1.0"))
	float LinearDamping = 0.94f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.7", ClampMax = "1.0"))
	float AngularDamping = 0.97f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float AngularSpringConstant = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float GravityScale = 0.0f;
	// Component-space inertia, supplied to Unreal's AnimDynamics node.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LinearInertiaScale = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AngularInertiaScale = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ToolTip = "Maximum head angular velocity passed to AnimDynamics, in radians per second."))
	float MaxAngularVelocity = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ToolTip = "Maximum head angular acceleration passed to AnimDynamics, in radians per second squared."))
	float MaxAngularAcceleration = 12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ToolTip = "Maximum component acceleration passed to AnimDynamics, in cm/s squared."))
	float MaxLinearAcceleration = 450.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "-1", ToolTip = "Last LOD that simulates. -1 enables all LODs; 1 simulates LOD 0 and 1."))
	int32 SimulationLODThreshold = 1;
	// Optional analytic scalp limit in hair component space; no Chaos bodies are needed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", Units = "cm"))
	float ScalpRadius = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Units = "cm"))
	FVector ScalpOffset = FVector::ZeroVector;
};

UCLASS(BlueprintType)
class STAGESHIFT_API UCharacterHairStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TObjectPtr<USkeletalMesh> Mesh;
	// A template Anim Blueprint can serve every hairstyle: bones come from Strands.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hair")
	TSubclassOf<UStageShiftHairAnimInstance> AnimationClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FName AttachmentSocket = TEXT("head");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FTransform AttachmentTransform;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	bool bEnablePhysics = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	FCharacterHairPhysicsSettings Physics;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physics")
	TArray<FCharacterHairStrand> Strands;
};
