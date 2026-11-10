// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "Core/StageShiftCharacter.h"
#include "PlayerCharacter.generated.h"

class UInputAction;
class UInputMappingContext;
class UInteractionComponent;
class UPhysicsManipulationComponent;
class USceneComponent;
class UThrowAimComponent;

UCLASS()
class STAGESHIFT_API APlayerCharacter : public AStageShiftCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void UnPossessed() override;
	
	virtual void Tick(float DeltaSeconds) override;
	virtual const UPrimitiveComponent* GetHeldPhysicsComponent() const override;
	
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
	TObjectPtr<USceneComponent> CarryPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UThrowAimComponent> ThrowAimComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	FName CarrySocketName = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta=(ClampMin = 0.0f, UIMin = 0.0f))
	float MovementBlendSpeed = 10.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float WalkSpeed = 150.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SprintSpeed = 500.0f;

public:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
private:
	UFUNCTION()
	void OnMovementInput(const FInputActionInstance& Instance);
	
	UFUNCTION()
	void OnCameraInput(const FInputActionInstance& Instance);
	
	UFUNCTION()
	void OnJumpInput(const FInputActionInstance& Instance);
	
	UFUNCTION()
	void OnSprintInput(const FInputActionInstance& Instance);
	
	float TargetSpeed = 0.0f;
};
