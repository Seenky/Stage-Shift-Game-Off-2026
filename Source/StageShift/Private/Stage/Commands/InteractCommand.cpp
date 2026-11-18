#include "Stage/Commands/InteractCommand.h"

#include "Physics/PhysicsActor.h"
#include "Stage/Actions/InteractAction.h"
#include "Stage/Actions/StageInteractionHelpers.h"
#include "Stage/Core/Data/StageDataAsset.h"

UInteractCommand::UInteractCommand()
{
	bRequireStageLocation = true;
	bRequireStageYaw = true;
}

void UInteractCommand::PostLoad()
{
	Super::PostLoad();
	if (TargetIds.IsEmpty() && !TargetId.IsNone() && !GraphParameterBindings.Contains(TEXT("TargetId")))
	{
		TargetIds.Add(TargetId);
		TargetId = NAME_None;
	}
}

TConstArrayView<FName> UInteractCommand::GetInteractionTargets() const
{
	if (TargetIds.IsEmpty() && !TargetId.IsNone()) return MakeArrayView(&TargetId, 1);
	return TargetIds;
}

void UInteractCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	const auto Targets = GetInteractionTargets();
	if (Targets.IsEmpty()) Errors.Add(Path + TEXT(" / Interact requires at least one Target Id"));
	for (const FName Id : Targets)
		if (Id.IsNone() || !Data || !Data->TargetIds.Contains(Id)) Errors.Add(Path + TEXT(" / undeclared Interact target: ") + Id.ToString());
	if (InteractionPhase != EInteractionPhase::Start && InteractionPhase != EInteractionPhase::End) Errors.Add(Path + TEXT(" / invalid interaction phase"));
	if (!FMath::IsFinite(HoldDuration) || HoldDuration < 0) Errors.Add(Path + TEXT(" / HoldDuration must be nonnegative and finite"));
	if (!FMath::IsFinite(MaxDistance) || MaxDistance <= 0) Errors.Add(Path + TEXT(" / MaxDistance must be positive and finite"));
}

void UInteractCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	for (const FName Id : GetInteractionTargets())
	{
		const AActor* Target = ResolveTarget(InContext, Id);
		if (!IsValid(Target) || !Target->Implements<UInteractionInterface>())
			Errors.Add(Path + TEXT(" / Interact target must implement InteractionInterface: ") + Id.ToString());
	}
}

float UInteractCommand::GetNeededTime_Implementation() const
{
	return InteractionPhase == EInteractionPhase::Start ? HoldDuration : 0.0f;
}

AActor* UInteractCommand::FindInteractionTarget(const FStageExecutionContext& InContext) const
{
	const auto Targets = GetInteractionTargets();
	AActor* Owner = StageInteraction::GetOwner(InContext);
	const auto* Manipulator = IsValid(Owner) ? Owner->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	if (Manipulator && Manipulator->IsCarrying())
	{
		for (const FName Id : Targets)
		{
			AActor* Target = ResolveTarget(InContext, Id);
			if (StageInteraction::IsHolding(Manipulator, Target)) return Target;
		}
	}

	AActor* BestTarget = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FName Id : Targets)
	{
		AActor* Target = ResolveTarget(InContext, Id);
		const auto* Item = Cast<AStagePhysicsActor>(Target);
		if (Item && Item->GetInteractionType() == EPhysicsInteractionType::Carry) continue;
		if (Targets.Num() == 1) return Target;
		FInteractionContext Interaction;
		if (StageInteraction::MakeContext(Owner, Target, ActionTag, InteractionPhase, MaxDistance, Interaction) &&
		    Interaction.Hit.Distance <= MaxDistance && Interaction.Hit.Distance < BestDistance &&
		    (InteractionPhase != EInteractionPhase::Start || IInteractionInterface::Execute_CanInteract(Target, Interaction)))
		{
			BestTarget = Target;
			BestDistance = Interaction.Hit.Distance;
		}
	}
	return BestTarget;
}

UStageAction* UInteractCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.IsValid() || !InContext.Manager.IsValid()) return nullptr;
	AActor* Target = FindInteractionTarget(InContext);
	auto* Action = NewObject<UInteractAction>(InContext.MemberComponent.Get());
	Action->SetupInteraction(Target, ActionTag, InteractionPhase, HoldDuration, MaxDistance);
	return Action;
}
