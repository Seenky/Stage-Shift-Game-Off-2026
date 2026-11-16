// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Actions/WaitForItemAction.h"

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

	// @todo bind to PhysicsManipulationComponent Delegate OnPickUp
}
