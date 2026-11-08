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
class UThrowAimComponent;

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
	virtual void Landed(const FHitResult& Hit) override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveInputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractInputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CameraInputAction = nullptr;
	
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UThrowAimComponent> ThrowAimComponent = nullptr;
	
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

	UPROPERTY(EditDefaultsOnly, Category = "Visual|Squash", meta=(ClampMin="0.0", ClampMax="0.03"))
	float StepSquash = 0.008f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual|Squash", meta=(ClampMin="0.0", ClampMax="0.05"))
	float LandingSquash = 0.025f;
	
public:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
private:
	void ConfigureMovementCollision();
	void UpdateSquash(float DeltaSeconds);

	UFUNCTION()
	void OnMovementInput(const FInputActionInstance& Instance);
	
	UFUNCTION()
	void OnCameraInput(const FInputActionInstance& Instance);
	
	UFUNCTION()
	void OnJumpInput(const FInputActionInstance& Instance);
	
	UFUNCTION()
	void OnSprintInput(const FInputActionInstance& Instance);
	
	float TargetSpeed = 0.0f;
	bool bStumblePending = false;
	FVector StumbleVelocity = FVector::ZeroVector;
	float SquashAmount = 0.0f;
	float SquashVelocity = 0.0f;
	FVector2D PreviousFootContact = FVector2D::ZeroVector;
	bool bWasGrounded = false;
};
