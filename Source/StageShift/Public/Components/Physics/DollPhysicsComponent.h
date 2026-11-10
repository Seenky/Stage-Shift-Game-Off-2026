#pragma once

#include "PhysicsControlComponent.h"
#include "DollPhysicsComponent.generated.h"

class AStageShiftCharacter;

USTRUCT(BlueprintType)
struct FStageShiftDollDrive
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.1", Units = "Hz"))
	float PositionFrequency = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.1", Units = "Hz"))
	float RotationFrequency = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float DampingRatio = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "100.0"))
	float MaxAcceleration = 5000.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "1.0"))
	float MaxAngularAcceleration = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityFeedForward = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Drive", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AngularVelocityFeedForward = 0.35f;
};

struct FStageShiftDollContact
{
	FName Bone;
	FVector LocalPoint = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	float Depth = 0.0f;
	float Velocity = 0.0f;
	float ImpactDepth = 0.0f;
	double LastContactTime = 0.0;
};

/** Finite-force animation tracking. Every contact body, including the pelvis, remains dynamic. */
UCLASS(ClassGroup = Physics, meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UDollPhysicsComponent : public UPhysicsControlComponent
{
	GENERATED_BODY()

public:
	UDollPhysicsComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsDriving() const { return bDriving; }
	void SuspendForRagdoll();

	UFUNCTION(BlueprintCallable, Category = "Doll Physics")
	void ResetAfterTeleport();
	FVector ConstrainMovement(const FVector& Delta) const;
	TConstArrayView<FStageShiftDollContact> GetContacts() const { return Contacts; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Doll Physics")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Drives")
	FStageShiftDollDrive PelvisDrive;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Drives")
	FStageShiftDollDrive BodyDrive;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Drives")
	FStageShiftDollDrive HeadDrive;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Drives")
	FStageShiftDollDrive ArmDrive;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Drives")
	FStageShiftDollDrive LegDrive;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Limits", meta = (ClampMin = "2.0", Units = "cm"))
	float RootLeash = 12.0f;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Limits", meta = (ClampMin = "2.0", Units = "cm"))
	float MaxTargetError = 18.0f;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Limits", meta = (ClampMin = "90.0", Units = "cm/s"))
	float MaxTargetSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Limits", meta = (ClampMin = "90.0", Units = "cm/s"))
	float MaxLimbTargetSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Interaction", meta = (ClampMin = "100.0"))
	float MaxInteractionAcceleration = 3500.0f;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Interaction", meta = (ClampMin = "1.0"))
	float MaxInteractionAngularAcceleration = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Doll Physics|Squash", meta = (ClampMin = "0.0", ClampMax = "5.0", Units = "cm"))
	float MaxContactDepth = 3.5f;

private:
	bool StartDriving();
	void StopDriving(bool bRestoreAnimation);
	void UpdateLegTargets();
	void UpdateLegJointLimits();
	void UpdateDriveTargets(float DeltaTime);
	void UpdateContacts(float DeltaTime);
	const FStageShiftDollDrive& GetDrive(FName Bone) const;

	UFUNCTION()
	void OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	struct FDrivenBody
	{
		FName Bone;
		FName Control;
		int32 JointIndex = INDEX_NONE;
		bool bLimitsWidened = false;
		FTransform PreviousTarget = FTransform::Identity;
		FTransform AnimationTarget = FTransform::Identity;
		bool bWasSimulating = false;
		bool bWasCCD = false;
		bool bWasNotifying = false;
		float BlendWeight = 0.0f;
	};

	TWeakObjectPtr<AStageShiftCharacter> Character;
	TWeakObjectPtr<USkeletalMeshComponent> Mesh;
	TArray<FDrivenBody> DrivenBodies;
	TArray<FStageShiftDollContact> Contacts;
	FTransform PreviousComponentTransform = FTransform::Identity;
	FVector InteractionNormal = FVector::ZeroVector;
	double LastDynamicContactTime = -1.0e10;
	FCollisionResponseContainer SavedResponses;
	TEnumAsByte<ECollisionChannel> SavedObjectType = ECC_Pawn;
	ECollisionEnabled::Type SavedCollision = ECollisionEnabled::NoCollision;
	EVisibilityBasedAnimTickOption SavedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	bool bDriving = false;
	bool bSuspended = false;
	bool bResetPending = false;
	bool bMeshSettingsSaved = false;
};
