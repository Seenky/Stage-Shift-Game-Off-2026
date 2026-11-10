#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/PoseSnapshot.h"
#include "StageShiftAnimInstance.generated.h"

class AStageShiftCharacter;

struct FStageShiftDollPose
{
	float Compression = 0.0f;
	float HeadCompression = 0.0f;
	float ContactOffset = 0.0f;
	FVector ContactNormal = FVector::ZeroVector;
	FVector ContactAnchor = FVector::ZeroVector;
	FName ContactBone = NAME_None;
	FVector ChestLean = FVector::ZeroVector;
	FVector HeadLean = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FStageShiftGaitSpeeds
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "1.0", Units = "cm/s"))
	float Walk = 117.55f;

	UPROPERTY(EditAnywhere, Category = "Locomotion", meta = (ClampMin = "1.0", Units = "cm/s"))
	float Run = 235.13f;
};

UCLASS(Blueprintable)
class STAGESHIFT_API UStageShiftAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	bool BlendFromRagdollPose(FPoseSnapshot&& Pose, const FTransform& PreviousMeshTransform, float BlendTime);
	void AddLandingSquash(float Strength);
	void UpdateContactSquash(const FVector& Away, const FVector& ImpactPoint, float ImpactSpeed, float Push, bool bNewContact,
		FName HitBodyBone = NAME_None);
	const FStageShiftDollPose& GetDollPose() const { return DollMotion.Pose; }

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Recovery")
	FPoseSnapshot RecoveryPose;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Recovery")
	float RecoveryPoseAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	float GroundSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	float Stride = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	float Gait = 0.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion")
	float PlayRate = 1.0f;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Locomotion|Foot IK")
	float FootIKAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion|Foot IK")
	bool bEnableFootIK = true;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Pickup")
	FVector PickupTarget = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector PickupHandOffset = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Pickup")
	float PickupAlpha = 0.0f;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Gait Speeds")
	FStageShiftGaitSpeeds ForwardSpeeds;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Gait Speeds")
	FStageShiftGaitSpeeds SideSpeeds = {37.5f, 69.16f};

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Gait Speeds")
	FStageShiftGaitSpeeds BackwardSpeeds = {71.88f, 138.34f};

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Play Rate", meta = (ClampMin = "0.01"))
	float MinPlayRate = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Play Rate", meta = (ClampMin = "0.01"))
	float MaxPlayRate = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion|Play Rate", meta = (ClampMin = "0.0"))
	float PlayRateResponse = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Doll Motion", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float ContactSquashScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Doll Motion", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MotionInertiaScale = 1.0f;

private:
	void UpdateDollMotion(const AStageShiftCharacter* Character, const FVector& PlanarVelocity, bool bGrounded, float DeltaSeconds);

	struct
	{
		FStageShiftDollPose Pose;
		FVector2D PreviousFootContact = FVector2D::ZeroVector;
		FVector PreviousVelocity = FVector::ZeroVector;
		double PreviousYaw = 0.0;
		bool bWasGrounded = false;
		float CompressionVelocity = 0.0f;
		float HeadCompressionVelocity = 0.0f;
		float ContactVelocity = 0.0f;
		float ContactHold = 0.0f;
		float ContactHoldTime = 0.0f;
		FVector LastProjectedImpact = FVector::ZeroVector;
		FVector ChestLeanVelocity = FVector::ZeroVector;
		FVector HeadLeanVelocity = FVector::ZeroVector;
	} DollMotion;

	float RecoveryBlendTime = 0.0f;
	float RecoveryBlendElapsed = 0.0f;
};
