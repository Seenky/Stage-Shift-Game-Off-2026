// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsTypes.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "StageShift/Public/Components/Interaction/InteractionInterface.h"
#include "PhysicsActor.generated.h"

class UPhysicsManipulationComponent;

UCLASS()
class STAGESHIFT_API AStagePhysicsActor : public AStaticMeshActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStagePhysicsActor();
	
	//** IInteractionInterface
	virtual bool CanInteract_Implementation(const FInteractionContext& InteractionContext) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& InteractionContext) override;
	virtual FName GetInteractionAnimationId_Implementation() const override;
	//** IInteractionInterface
	
	UFUNCTION(BlueprintPure)
	EPhysicsInteractionType GetInteractionType() const { return InteractionType; }
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EPhysicsInteractionType InteractionType = EPhysicsInteractionType::None;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UPhysicsManipulationComponent> ActiveManipulator;
};
