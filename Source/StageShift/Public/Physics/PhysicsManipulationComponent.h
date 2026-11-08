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
	void SetThrowDirection(const FVector& Direction) { ThrowDirection = Direction.GetSafeNormal2D(); }

	bool GetPickupTarget(FVector& WorldLocation, float& Weight, FVector* HandOffset = nullptr) const;

	virtual void ReleaseComponent() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Drag", meta = (ClampMin = "1.0", Units = "cm"))
	float DragDistance = 100.0f;
	
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
	FVector DragTarget = FVector::ZeroVector;
	float DragHeightOffset = 0.0f;
	FVector LocalGripLocation = FVector::ZeroVector;
	FVector GrabLocation = FVector::ZeroVector;
	FVector ThrowDirection = FVector::ZeroVector;
	TWeakObjectPtr<class UAnimInstance> PickupInstance;
	TWeakObjectPtr<class UAnimMontage> PickupMontage;
	bool bPickupPending = false;
	bool bCarryAttached = false;

	bool AttachCarriedObject(bool bKeepWorldTransform);
	void ClearPickupPlayback();

	UFUNCTION()
	void OnPickupNotify(FName Name, const struct FBranchingPointNotifyPayload& Payload);

	UFUNCTION()
	void OnPickupMontageEnded(class UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnManipulatedActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);
};
