// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "PhysicsTypes.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "PhysicsManipulationComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STAGESHIFT_API UPhysicsManipulationComponent : public UPhysicsHandleComponent
{
	GENERATED_BODY()

public:
	UPhysicsManipulationComponent();

	UFUNCTION(BlueprintCallable, Category = "Physics Interaction")
	void SetCarryPoint(USceneComponent* NewCarryPoint);

	UFUNCTION(BlueprintPure, Category = "Physics Interaction")
	bool CanManipulate(const FHitResult& Hit, EPhysicsInteractionType Type) const;

	UFUNCTION(BlueprintCallable, Category = "Physics Interaction")
	bool BeginManipulation(const FHitResult& Hit, EPhysicsInteractionType Type);

	UFUNCTION(BlueprintCallable, Category = "Physics Interaction")
	void EndManipulation(float HoldDuration, bool bCancelled);

	UFUNCTION(BlueprintPure, Category = "Physics Interaction")
	UPrimitiveComponent* GetManipulatedComponent() const { return ManipulatedComponent.Get(); }
	bool IsCarrying() const { return bCarryAttached; }
	bool IsDragging() const { return ActiveType == EPhysicsInteractionType::Drag && ManipulatedComponent.IsValid(); }
	float GetDragMovementScale() const;
	FVector ConstrainDragMovement(const FVector& Delta) const;
	bool GetDragPose(FVector& LeftHand, FVector& RightHand, FVector& Facing, float& Effort) const;
	void SetThrowDirection(const FVector& Direction) { ThrowDirection = Direction.GetSafeNormal2D(); bHasThrowTarget = false; }

	UFUNCTION(BlueprintCallable, Category = "Physics Interaction|Throw")
	void SetThrowTarget(const FVector& WorldLocation, const FVector& SurfaceNormal = FVector::UpVector);

	void ClearThrowTarget() { bHasThrowTarget = false; }

	bool GetPickupTarget(FVector& WorldLocation, float& Weight, FVector* HandOffset = nullptr) const;

	virtual void ReleaseComponent() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnUnregister() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drag", meta = (ClampMin = "1.0", Units = "cm"))
	float DragDistance = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "1.0", Units = "cm"))
	float DragReachSlack = 24.0f;

	// Force at a 25 kg reference load, in Newtons. Scales with DragReferenceMass.
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "1.0", Units = "N"))
	float DragMaxForce = 650.0f;
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "1.0"))
	float DragMaxAcceleration = 1800.0f; // cm/s^2; limits light props as well as the absolute force limit.
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "1.0"))
	float DragSpringStiffness = 1800.0f;
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float DragDampingRatio = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "0.0"))
	float DragAngularDamping = 3.0f;
	UPROPERTY(EditAnywhere, Category = "Drag|Turning", meta = (ClampMin = "1.0", Units = "deg/s"))
	float DragTurnSpeed = 90.0f; // Grip orbit speed around the character.
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "0.1", Units = "kg"))
	// Raising the reference load increases pulling strength and spring stiffness as well as movement speed.
	float DragReferenceMass = 25.0f;
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float DragSpeedMultiplier = 0.95f;
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float DragMinimumSpeedMultiplier = 0.10f;
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "1.0", Units = "cm"))
	float DragHandSpacing = 32.0f;
	UPROPERTY(EditAnywhere, Category = "Drag", meta = (ClampMin = "1.0", Units = "cm"))
	float DragBreakDistance = 180.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "0.0", Units = "s"))
	float TapDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "0.01", Units = "s"))
	float MaxChargeDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MinThrowSpeed = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MaxThrowSpeed = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throw", meta = (ClampMin = "0.0"))
	float ThrowUpwardBias = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	FName GripSocket = TEXT("Grip");

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> CarryPoint;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> ManipulatedComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ManipulatedActor;

	EPhysicsInteractionType ActiveType = EPhysicsInteractionType::None;
	ECollisionEnabled::Type PreviousCollision = ECollisionEnabled::QueryAndPhysics;
	bool bWasSimulatingPhysics = false;
	TWeakObjectPtr<class ACharacter> DragCharacter;
	FName DragBoneName;
	FVector DragLocalHands[2] = {FVector::ZeroVector, FVector::ZeroVector};
	FVector DragLocalGripNormal = FVector::ZeroVector;
	float DragTargetYaw = 0.0f;
	float DragMass = 0.0f;
	float DragRestDistance = 0.0f;
	float DragReach = 0.0f;
	float DragEffort = 0.0f;
	float SavedDragAngularDamping = 0.0f;
	float AppliedDragAngularDamping = 0.0f;
	bool bSavedOrientToMovement = false;
	bool bSavedControllerDesiredRotation = false;
	bool bSavedControllerYaw = false;
	FVector LocalGripLocation = FVector::ZeroVector;
	FVector GrabLocation = FVector::ZeroVector;
	FVector ThrowDirection = FVector::ZeroVector;
	FVector ThrowTarget = FVector::ZeroVector;
	FVector ThrowTargetNormal = FVector::UpVector;
	bool bHasThrowTarget = false;
	TWeakObjectPtr<class UAnimInstance> PickupInstance;
	TWeakObjectPtr<class UAnimMontage> PickupMontage;
	bool bPickupPending = false;
	bool bCarryAttached = false;

	bool AttachCarriedObject(bool bKeepWorldTransform);
	void BeginDrag(const FHitResult& Hit);
	void SetDragGrip(const FVector& GripPoint);
	void UpdateDrag(float DeltaTime);
	void CancelManipulation();
	FTransform GetDragFrame() const;
	void ClearPickupPlayback();
	FVector CalculateThrowVelocity(const UPrimitiveComponent& Component, const FVector& Target, float Speed) const;

	UFUNCTION()
	void OnPickupNotify(FName Name, const struct FBranchingPointNotifyPayload& Payload);

	UFUNCTION()
	void OnPickupMontageEnded(class UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnManipulatedActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);
};
