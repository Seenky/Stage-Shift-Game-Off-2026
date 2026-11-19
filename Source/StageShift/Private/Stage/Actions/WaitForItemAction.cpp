// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Actions/WaitForItemAction.h"

#include "Physics/PhysicsManipulationComponent.h"

void UWaitForItemAction::SetupWaitForItem(
	UPhysicsManipulationComponent* InPhysicsManipulationComponent,
	AActor* InItem)
{
	PhysicsManipulationComponent = InPhysicsManipulationComponent;
	NeededItem = InItem;
}

void UWaitForItemAction::OnStart_Implementation()
{
	if (!PhysicsManipulationComponent.IsValid() || !NeededItem.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	if (PhysicsManipulationComponent->IsCarrying() &&
		PhysicsManipulationComponent->GetManipulatedComponent() &&
		PhysicsManipulationComponent->GetManipulatedComponent()->GetOwner() &&
		PhysicsManipulationComponent->GetManipulatedComponent()->GetOwner() == NeededItem)
	{
		Finish(EStageActionResult::Succeeded);
		return;
	}

	PhysicsManipulationComponent->OnItemPickedUp.AddDynamic(this, &UWaitForItemAction::OnPickupItem);
}

void UWaitForItemAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (Mode == EStageActionStopMode::StopAction)
	{
		if (GetState() != EStageExecutionState::Finished) Finish(EStageActionResult::Interrupted);
	}
}

void UWaitForItemAction::Cleanup_Implementation()
{
	if (PhysicsManipulationComponent.IsValid())
	{
		PhysicsManipulationComponent->OnItemPickedUp.RemoveDynamic(this, &UWaitForItemAction::OnPickupItem);
	}
}

void UWaitForItemAction::OnPickupItem(AActor* InItem)
{
	if (InItem && InItem == NeededItem)
	{
		Finish(EStageActionResult::Succeeded);
	}
}
