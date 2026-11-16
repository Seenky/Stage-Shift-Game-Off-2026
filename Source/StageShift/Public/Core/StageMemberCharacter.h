// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageShiftCharacter.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Components/Dialogue/DialogueComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "StageMemberCharacter.generated.h"

UCLASS()
class STAGESHIFT_API AStageMemberCharacter : public AStageShiftCharacter
{
	GENERATED_BODY()

public:
	AStageMemberCharacter();

	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStageMemberComponent> StageMemberComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStatusesComponent> StatusesComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Dialogue")
	TObjectPtr<UWidgetComponent> DialogueWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Dialogue")
	TObjectPtr<UDialogueComponent> DialogueComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Dialogue")
	TObjectPtr<UPhysicsManipulationComponent> PhysicsManipulationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|Dialogue")
	TObjectPtr<UInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> CarryPoint = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	FName CarrySocketName = TEXT("hand_r");
};
