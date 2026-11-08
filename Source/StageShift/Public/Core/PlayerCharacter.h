// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "GameFramework/Character.h"
#include "PlayerCharacter.generated.h"

class UPhysicalMontageComponent;
class UInputAction;
class UInputMappingContext;
class UInteractionComponent;
class UPhysicsManipulationComponent;
class USceneComponent;

UCLASS()
class STAGESHIFT_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void UnPossessed() override;
	
	virtual void Tick(float DeltaSeconds) override;
	virtual void MoveBlockedBy(const FHitResult& Impact) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveInputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractInputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpInputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintInputAction = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInteractionComponent> InteractionComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPhysicsManipulationComponent> PhysicsManipulationComponent = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPhysicalMontageComponent> PhysicalMontageComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> CarryPoint = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	FName CarrySocketName = TEXT("hand_r");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	TSet<FName> HitBoneNames = {};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta=(ClampMin = 0.0f, UIMin = 0.0f))
	float MovementBlendSpeed = 10.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 150.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 500.0f;
	
public:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
private:
	void ConfigureMovementCollision();

	UFUNCTION()
	void OnMovementInput(const FInputActionInstance& Instance);
	
	UFUNCTION()
	void OnJumpInput(const FInputActionInstance& Instance);
	
	UFUNCTION()
	void OnSprintInput(const FInputActionInstance& Instance);
	
	float TargetSpeed = 0.0f;
	bool bStumblePending = false;
	FVector StumbleVelocity = FVector::ZeroVector;
};
