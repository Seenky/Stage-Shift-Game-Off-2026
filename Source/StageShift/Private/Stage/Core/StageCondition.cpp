#include "Stage/Core/StageCondition.h"

#include "Physics/PhysicsManipulationComponent.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Core/StageMemberCommand.h"
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
	if (!Manipulation || !Manipulation->IsCarrying() || !IsValid(Item)) return false;
	const AActor* HeldActor = Item->GetOwner();
	if (!TargetIds.IsEmpty())
		return TargetIds.ContainsByPredicate([Manager, HeldActor](FName Id) { return HeldActor == Manager->GetTarget(Id); });
	return TargetId.IsNone() || HeldActor == Manager->GetTarget(TargetId);
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
	if (TargetIds.IsEmpty())
	{
		if (!TargetId.IsNone() && (!Data || !Data->TargetIds.Contains(TargetId))) Errors.Add(Path + TEXT(" / Has Held Item requires a declared Target Id"));
	}
	else
	{
		for (FName Id : TargetIds)
			if (Id.IsNone() || !Data || !Data->TargetIds.Contains(Id)) Errors.Add(Path + TEXT(" / Has Held Item requires declared Target Ids"));
	}
}

TArray<FName> UStageStepCommandStartedCondition::GetStepOptions() const
{
	const auto* Data = GetTypedOuter<UStageDataAsset>();
	return Data ? Data->GetStepOptions() : TArray<FName>{};
}

bool UStageStepCommandStartedCondition::Evaluate_Implementation(AStageManager* Manager) const
{
	const FStageStep* Step = IsValid(Manager) ? Manager->FindPreparedStep(StepId) : nullptr;
	if (!Step || !CommandClass) return false;
	const auto WasStarted = [this](auto&& Search, const UStageMemberCommand* Command) -> bool
	{
		if (!IsValid(Command)) return false;
		if (Command->IsA(CommandClass) && Command->HasStartedMainAction()) return true;
		return Command->GetChildCommands().ContainsByPredicate([&](const UStageMemberCommand* Child) { return Search(Search, Child); });
	};
	return Step->Commands.ContainsByPredicate([&](const FStageMemberAssignment& Assignment) { return WasStarted(WasStarted, Assignment.Command); });
}

void UStageStepCommandStartedCondition::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	if (!Data || StepId.IsNone() || Data->FindStepIndex(StepId) == INDEX_NONE || !CommandClass)
		Errors.Add(Path + TEXT(" / Step Command Started needs a declared step and a Command class"));
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
