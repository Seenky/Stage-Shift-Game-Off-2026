#pragma once

#include "GameFramework/Character.h"
#include "StageShiftCharacter.generated.h"

class UMeshDeformer;
class UDollPhysicsComponent;
class UPhysicalMontageComponent;
class UDialogueComponent;
class UFacialAnimationComponent;
class UWidgetComponent;

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
	TObjectPtr<UDollPhysicsComponent> DollPhysicsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Dialogue")
	TObjectPtr<UDialogueComponent> DialogueComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Dialogue")
	TObjectPtr<UWidgetComponent> DialogueWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Face")
	TObjectPtr<UFacialAnimationComponent> FacialAnimationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|Meshes")
	TObjectPtr<USkeletalMeshComponent> HairMesh;

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

private:
	friend class UStageShiftCharacterMovementComponent;
	friend class UDollPhysicsComponent;

	void ConfigureMovementCollision();
	void UpdateDollPress();
	void ApplyDollContact(const FHitResult& Impact);
	void ConsiderStumble(const FHitResult& Impact);

	bool bStumblePending = false;
	FVector StumbleVelocity = FVector::ZeroVector;
	TWeakObjectPtr<UPrimitiveComponent> LastSquashObstacle;
	double LastSquashContactTime = 0.0;
	FDelegateHandle DollPressHandle;
};
