// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Commands/WaitForItemCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Core/Data/StageDataAsset.h"

void UWaitForItemCommand::ValidateDefinition(const class UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (!ItemId.IsNone() && !Data->TargetIds.Contains(ItemId)) Errors.Add(Path + TEXT(" / unknown ItemId: ") + ItemId.ToString());
	if (ItemId.IsNone()) Errors.Add(Path + TEXT(" / ItemId is required"));
}

bool UWaitForItemCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	if (!InContext.MemberComponent.Get() ||
		!InContext.MemberComponent.Get()->GetOwner() ||
		!InContext.Manager.IsValid())
	{
		return false;
	}

	const auto NeededItem = InContext.Manager->GetTarget(ItemId);
	if (!NeededItem) return false;

	UPhysicsManipulationComponent* PhysicsManipulationComponent = InContext.MemberComponent.Get()->GetOwner()->FindComponentByClass<UPhysicsManipulationComponent>();
	if (!PhysicsManipulationComponent) return false;

	if (PhysicsManipulationComponent->IsCarrying() &&
		PhysicsManipulationComponent->GetManipulatedComponent() &&
		PhysicsManipulationComponent->GetManipulatedComponent()->GetOwner() &&
		PhysicsManipulationComponent->GetManipulatedComponent()->GetOwner() == NeededItem)
	{
		return true;
	}

	return false;
}
