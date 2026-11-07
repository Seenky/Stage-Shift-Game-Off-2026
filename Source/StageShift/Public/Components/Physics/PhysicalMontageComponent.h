// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "PhysicalMontageComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STAGESHIFT_API UPhysicalMontageComponent : public UPhysicalAnimationComponent
{
	GENERATED_BODY()

public:
	UPhysicalMontageComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void StartPhysicsMontage(FName MontageName);
	bool IsPhysicsMontageActive() const { return bMeshCollisionSaved || bIsRecovering; }

	UFUNCTION(BlueprintCallable)
	void ActivatePhysics(FName MontageName);

	UFUNCTION(BlueprintCallable)
	void RestorePhysics();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TMap<FName, UAnimMontage*> AnimMontages = {};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TObjectPtr<UAnimMontage> RecoveryMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FPhysicalAnimationData PhysicalAnimationData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = 0, UIMin = 0))
	float BlendWeight = 0.5f;

	/** Seconds from the start of the jump until physics takes over. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = "0.0", Units = "s"))
	float PhysicsActivationDelay = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = 0.1, UIMin = 0.1))
	float TimeToRecovery = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = "0.0", Units = "s"))
	float RecoveryPoseBlendTime = 0.28f;

private:
	bool PreparePhysics(FName MontageName);
	void RestoreMeshCollision();
	void OnRecoveryMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	FTimerHandle ActivationTimerHandle;
	FTimerHandle RecoveryTimerHandle;
	FTransform MeshRelativeTransform;
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
