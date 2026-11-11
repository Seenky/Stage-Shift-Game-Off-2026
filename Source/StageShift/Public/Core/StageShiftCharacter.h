#pragma once

#include "GameFramework/Character.h"
#include "Animation/CharacterPhysicsAnimationSet.h"
#include "StageShiftCharacter.generated.h"

class UMeshDeformer;
class UDollPhysicsComponent;
class UPhysicalMontageComponent;
class UCharacterPhysicsReactionComponent;
class UDialogueComponent;
class UFacialAnimationComponent;
class UWidgetComponent;
class UCharacterHairComponent;

UCLASS()
class STAGESHIFT_API AStageShiftCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AStageShiftCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	float GetStepSquash() const { return StepSquash; }
	bool IsPhysicsMontageActive() const;
	bool IsDollPhysicsActive() const;
	UDollPhysicsComponent* GetDollPhysics() const { return DollPhysicsComponent; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void MoveBlockedBy(const FHitResult& Impact) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual const UPrimitiveComponent* GetHeldPhysicsComponent() const { return nullptr; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPhysicalMontageComponent> PhysicalMontageComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Physics")
	TObjectPtr<UCharacterPhysicsReactionComponent> PhysicsReactionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Physics")
	TObjectPtr<UDollPhysicsComponent> DollPhysicsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Dialogue")
	TObjectPtr<UDialogueComponent> DialogueComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Dialogue")
	TObjectPtr<UWidgetComponent> DialogueWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Face")
	TObjectPtr<UFacialAnimationComponent> FacialAnimationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Meshes")
	TObjectPtr<UCharacterHairComponent> HairMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Meshes")
	TObjectPtr<USkeletalMeshComponent> HatMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Meshes")
	TObjectPtr<USkeletalMeshComponent> OutfitMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Visual|Squash")
	TObjectPtr<UMeshDeformer> DollPressDeformer;

	UPROPERTY(EditDefaultsOnly, Category = "Visual|Squash", meta = (ClampMin = "0.0", ClampMax = "0.03"))
	float StepSquash = 0.014f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual|Squash", meta = (ClampMin = "0.0", ClampMax = "0.05"))
	float LandingSquash = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Physics Hit")
	bool bEnablePhysicsHitKnockdown = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Physics Hit", meta = (ClampMin = "1.0", Units = "cm/s", EditCondition = "bEnablePhysicsHitKnockdown"))
	float PhysicsHitMinSpeed = 400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Physics Hit", meta = (ClampMin = "0.0", Units = "kg", EditCondition = "bEnablePhysicsHitKnockdown"))
	float PhysicsHitMinMass = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Physics Hit", meta = (ClampMin = "0.0", EditCondition = "bEnablePhysicsHitKnockdown", ToolTip = "Minimum collision impulse in Newton seconds. Set to zero to use only the speed and mass thresholds."))
	float PhysicsHitMinImpulse = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Physics Hit", meta = (ClampMin = "0.0", EditCondition = "bEnablePhysicsHitKnockdown"))
	float PhysicsHitKnockbackScale = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Physics Hit", meta = (ClampMin = "0.0", Units = "cm/s", EditCondition = "bEnablePhysicsHitKnockdown"))
	float PhysicsHitMaxKnockbackSpeed = 600.0f;

private:
	friend class UStageShiftCharacterMovementComponent;
	friend class UDollPhysicsComponent;

	void ConfigureMovementCollision();
	void UpdateDollPress();
	void ApplyDollContact(const FHitResult& Impact);
	void ConsiderStumble(const FHitResult& Impact);
	void ConsiderPhysicsHit(const FHitResult& Impact, const FVector& NormalImpulse);

	bool bStumblePending = false;
	FPhysicalAnimationInput PendingPhysicsInput;
	FVector StumbleDirection = FVector::ForwardVector;
	ECharacterPhysicsReaction PendingPhysicsReaction = ECharacterPhysicsReaction::Stumble;
	TWeakObjectPtr<UPrimitiveComponent> LastSquashObstacle;
	double LastSquashContactTime = 0.0;
	FDelegateHandle DollPressHandle;
};
