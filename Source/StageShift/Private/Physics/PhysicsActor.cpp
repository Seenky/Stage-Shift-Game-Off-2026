// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Physics/PhysicsActor.h"

#include "Components/StaticMeshComponent.h"
#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"

AStagePhysicsActor::AStagePhysicsActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	GetStaticMeshComponent()->SetCollisionProfileName(TEXT("PhysicsActor"));
	GetStaticMeshComponent()->SetSimulatePhysics(true);
	SetMobility(EComponentMobility::Movable);
}

bool AStagePhysicsActor::CanInteract_Implementation(const FInteractionContext& InteractionContext) const
{
	if (!InteractionContext.Interactor || InteractionContext.Hit.GetComponent() != GetStaticMeshComponent() 
		|| (ActiveManipulator.IsValid() && ActiveManipulator->GetManipulatedComponent() == GetStaticMeshComponent()))
	{
		return false;
	}
	const UPhysicsManipulationComponent* Manipulator = InteractionContext.Interactor->FindComponentByClass<UPhysicsManipulationComponent>();
	return Manipulator && Manipulator->CanManipulate(InteractionContext.Hit, InteractionType);
}

EInteractionResult AStagePhysicsActor::OnInteraction_Implementation(const FInteractionContext& InteractionContext)
{
	if (UPhysicsManipulationComponent* Manipulator = ActiveManipulator.Get())
	{
		if (Manipulator->GetOwner() != InteractionContext.Interactor) return EInteractionResult::Rejected;
		
		if (InteractionContext.InteractionPhase == EInteractionPhase::End)
		{
			ActiveManipulator.Reset();
			if (Manipulator->GetManipulatedComponent() == GetStaticMeshComponent())
			{
				Manipulator->EndManipulation(InteractionContext.HoldDuration, InteractionContext.bCancelled);
			}
			return EInteractionResult::Completed;
		}
		
		return Manipulator->GetManipulatedComponent() == GetStaticMeshComponent() ? EInteractionResult::Active : EInteractionResult::Rejected;
	}

	if (InteractionContext.InteractionPhase != EInteractionPhase::Start || !CanInteract_Implementation(InteractionContext))
	{
		return EInteractionResult::Rejected;
	}
	
	UPhysicsManipulationComponent* Manipulator = InteractionContext.Interactor->FindComponentByClass<UPhysicsManipulationComponent>();
	
	if (!Manipulator->BeginManipulation(InteractionContext.Hit, InteractionType)) return EInteractionResult::Rejected;
	
	ActiveManipulator = Manipulator;
	return InteractionType == EPhysicsInteractionType::Carry ? EInteractionResult::Latched : EInteractionResult::Active;
}

void AStagePhysicsActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPhysicsManipulationComponent* Manipulator = ActiveManipulator.Get())
	{
		if (Manipulator->GetManipulatedComponent() == GetStaticMeshComponent()) Manipulator->ReleaseComponent();
	}
	ActiveManipulator.Reset();
	Super::EndPlay(EndPlayReason);
}
