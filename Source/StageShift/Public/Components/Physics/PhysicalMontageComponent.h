// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicalMontageComponent.generated.h"

USTRUCT(BlueprintType)
struct STAGESHIFT_API FPhysicalMontageSettings
{
	GENERATED_BODY()

	FPhysicalMontageSettings();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> Montage = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float LaunchDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0", Units = "s"))
	float PhysicsActivationDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (Units = "cm/s"))
	FVector LaunchVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ClampMin = "0.0"))
	float InheritedVelocityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics")
	FPhysicalAnimationData PhysicalAnimationData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0"))
	float StrengthMultiplier = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhysicsBlendWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recovery", meta = (ClampMin = "0.1", Units = "s"))
	float TimeToRecovery = 1.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STAGESHIFT_API UPhysicalMontageComponent : public UPhysicalAnimationComponent
{
	GENERATED_BODY()

public:
	UPhysicalMontageComponent();

	virtual void PostLoad() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void StartPhysicsMontage(FName MontageName);
	void StartPhysicsMontage(FName MontageName, const FVector& InitialVelocity);
	bool IsPhysicsMontageActive() const { return bMeshCollisionSaved || bIsRecovering; }

	UFUNCTION(BlueprintCallable)
	void ActivatePhysics(FName MontageName);

	UFUNCTION(BlueprintCallable)
	void RestorePhysics();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TMap<FName, FPhysicalMontageSettings> MontageSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TObjectPtr<UAnimMontage> RecoveryMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = "0.0", Units = "s"))
	float RecoveryPoseBlendTime = 0.28f;

private:
	bool PreparePhysics(FName MontageName, const FVector& InitialVelocity);
	void LaunchMontage();
	void RestoreMeshCollision();
	void OnRecoveryMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UPROPERTY()
	FPhysicalAnimationData PhysicalAnimationData;

	UPROPERTY()
	float PhysicsActivationDelay = 0.12f;

	UPROPERTY()
	float TimeToRecovery = 1.0f;

	UPROPERTY(Transient)
	FPhysicalMontageSettings ActiveSettings;

	FTimerHandle LaunchTimerHandle;
	FTimerHandle ActivationTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FTransform MeshRelativeTransform;
	FVector MontageLaunchVelocity = FVector::ZeroVector;
	FName ActiveMontageName;
	FCollisionResponseContainer CurrentCollisionResponses;
	TEnumAsByte<ECollisionChannel> CurrentCollisionObjectType = ECC_Pawn;
	ECollisionResponse SavedCapsulePhysicsBodyResponse = ECR_Ignore;
	float SavedStrength = 1.0f;
	bool bMeshCollisionSaved = false;
	FName CurrentCollisionProfile = TEXT("");
	TEnumAsByte<ECollisionEnabled::Type> CurrentCollisionEnabled = ECollisionEnabled::NoCollision;
	bool bIsSimActive = false;
	bool bIsRecovering = false;
};
