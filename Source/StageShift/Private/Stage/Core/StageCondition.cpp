#include "Stage/Core/StageCondition.h"

#include "Physics/PhysicsManipulationComponent.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"

bool UStageCondition::Evaluate_Implementation(AStageManager* Manager) const
{
	return false;
}

void UStageCondition::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
}

TArray<FName> UStageHasHeldItemCondition::GetRoleOptions() const
{
	const auto* Data = GetTypedOuter<UStageDataAsset>();
	return Data ? Data->GetRoleOptions() : TArray<FName>{};
}

bool UStageHasHeldItemCondition::Evaluate_Implementation(AStageManager* Manager) const
{
	const AActor* Actor = IsValid(Manager) ? Manager->GetParticipant(RoleId) : nullptr;
	const auto* Manipulation = IsValid(Actor) ? Actor->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	return Manipulation && Manipulation->IsCarrying() && IsValid(Manipulation->GetManipulatedComponent());
}

void UStageHasHeldItemCondition::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	if (RoleId.IsNone() || !Data || !Data->GetRoleOptions().Contains(RoleId))
		Errors.Add(Path + TEXT(" / Has Held Item requires a declared Role Id"));
}
