#include "Stage/Core/StageCondition.h"

#include "Physics/PhysicsManipulationComponent.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"
#include "Components/PrimitiveComponent.h"
#include "StatusesComponent.h"

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
	const UPrimitiveComponent* Item = Manipulation ? Manipulation->GetManipulatedComponent() : nullptr;
	return Manipulation && Manipulation->IsCarrying() && IsValid(Item) && (TargetId.IsNone() || Item->GetOwner() == Manager->GetTarget(TargetId));
}

TArray<FName> UStageHasHeldItemCondition::GetTargetOptions() const
{
	const auto* Data = GetTypedOuter<UStageDataAsset>();
	return Data ? Data->GetTargetOptions() : TArray<FName>{};
}

void UStageHasHeldItemCondition::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	if (RoleId.IsNone() || !Data || !Data->GetRoleOptions().Contains(RoleId))
		Errors.Add(Path + TEXT(" / Has Held Item requires a declared Role Id"));
	if (!TargetId.IsNone() && (!Data || !Data->TargetIds.Contains(TargetId))) Errors.Add(Path + TEXT(" / Has Held Item requires a declared Target Id"));
}

TArray<FName> UStageRoleHasStatusCondition::GetRoleOptions() const
{
	const auto* Data = GetTypedOuter<UStageDataAsset>();
	return Data ? Data->GetRoleOptions() : TArray<FName>{};
}

bool UStageRoleHasStatusCondition::Evaluate_Implementation(AStageManager* Manager) const
{
	const AActor* Actor = IsValid(Manager) ? Manager->GetParticipant(RoleId) : nullptr;
	const auto* Statuses = IsValid(Actor) ? Actor->FindComponentByClass<UStatusesComponent>() : nullptr;
	return Statuses && Status.IsValid() && Statuses->GetStatuses().HasTag(Status) == bExpected;
}

void UStageRoleHasStatusCondition::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	if (!Data || !Data->GetRoleOptions().Contains(RoleId) || !Status.IsValid()) Errors.Add(Path + TEXT(" / Role Has Status needs a declared role and a valid status"));
}
