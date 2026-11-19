#include "Stage/Commands/PickUpCommand.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Physics/PhysicsActor.h"
#include "Stage/Actions/PickUpAction.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Actions/StageInteractionHelpers.h"

UPickUpCommand::UPickUpCommand()
{
	bRequireStageLocation = true;
	bRequireStageYaw = true;
}

void UPickUpCommand::PostLoad()
{
	Super::PostLoad();
	if (TargetIds.IsEmpty() && !TargetId.IsNone() && !GraphParameterBindings.Contains(TEXT("TargetId")))
	{
		TargetIds.Add(TargetId);
		TargetId = NAME_None;
	}
}

TConstArrayView<FName> UPickUpCommand::GetPickupTargets() const
{
	if (TargetIds.IsEmpty() && !TargetId.IsNone()) return MakeArrayView(&TargetId, 1);
	return TargetIds;
}

void UPickUpCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	const TConstArrayView<FName> Targets = GetPickupTargets();
	if (Targets.IsEmpty()) Errors.Add(Path + TEXT(" / Pick Up requires at least one Target Id"));
	for (const FName Id : Targets)
		if (Id.IsNone() || !Data || !Data->TargetIds.Contains(Id)) Errors.Add(Path + TEXT(" / undeclared Pick Up target: ") + Id.ToString());
	if (!FMath::IsFinite(MaxDistance) || MaxDistance <= 0) Errors.Add(Path + TEXT(" / MaxDistance must be positive and finite"));
}

void UPickUpCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	for (const FName Id : GetPickupTargets())
	{
		const auto* Item = Cast<AStagePhysicsActor>(ResolveTarget(InContext, Id));
		if (!Item || Item->GetInteractionType() != EPhysicsInteractionType::Carry)
			Errors.Add(Path + TEXT(" / Pick Up target must be a StagePhysicsActor with Carry interaction: ") + Id.ToString());
	}
	const AActor* Owner = StageInteraction::GetOwner(InContext);
	if (IsValid(Owner) && !Owner->FindComponentByClass<UPhysicsManipulationComponent>()) Errors.Add(Path + TEXT(" / PickUp requires PhysicsManipulationComponent and a configured CarryPoint"));
}

void UPickUpCommand::OnPreStart_Implementation(const FStageExecutionContext& InContext)
{
	NeededTime = 0.0f;
	AActor* Owner = StageInteraction::GetOwner(InContext);
	for (const FName Id : GetPickupTargets())
	{
		const auto* Animation = StageInteraction::FindAnimation(Owner, ResolveTarget(InContext, Id));
		if (!Animation || !Animation->Montage) continue;
		const UAnimMontage* Montage = Animation->Montage;
		if (!Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event) { return Event.Notify && Event.Notify->GetNotifyName() == TEXT("GrabObject"); })) continue;
		const int32 Section = Montage->GetSectionIndex(Animation->StartSection);
		const float Duration = Section == INDEX_NONE ? Montage->GetPlayLength() : Montage->GetSectionLength(Section);
		NeededTime = FMath::Max(NeededTime, Duration);
	}
}

float UPickUpCommand::GetNeededTime_Implementation() const
{
	return NeededTime;
}

bool UPickUpCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	const AActor* Owner = StageInteraction::GetOwner(InContext);
	const auto* Manipulator = IsValid(Owner) ? Owner->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	if (!Manipulator || !Manipulator->IsCarrying()) return false;
	for (const FName Id : GetPickupTargets())
		if (StageInteraction::IsHolding(Manipulator, ResolveTarget(InContext, Id))) return true;
	return false;
}

AActor* UPickUpCommand::FindPickupTarget(const FStageExecutionContext& InContext) const
{
	const auto Targets = GetPickupTargets();
	if (Targets.Num() == 1) return ResolveTarget(InContext, Targets[0]);
	AActor* Owner = StageInteraction::GetOwner(InContext);
	AActor* BestTarget = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const FName Id : Targets)
	{
		auto* Item = Cast<AStagePhysicsActor>(ResolveTarget(InContext, Id));
		if (!Item || Item->GetInteractionType() != EPhysicsInteractionType::Carry) continue;
		if (StageInteraction::GetOtherItemHolder(Owner, Item)) continue;
		FInteractionContext Interaction;
		if (StageInteraction::MakeContext(Owner, Item, FGameplayTag(), EInteractionPhase::Start, MaxDistance, Interaction) &&
		    Interaction.Hit.Distance < BestDistance && IInteractionInterface::Execute_CanInteract(Item, Interaction))
		{
			BestTarget = Item;
			BestDistance = Interaction.Hit.Distance;
		}
	}
	return BestTarget;
}

UStageAction* UPickUpCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.IsValid() || !InContext.Manager.IsValid()) return nullptr;
	AActor* Target = FindPickupTarget(InContext);
	auto* Action = NewObject<UPickUpAction>(InContext.MemberComponent.Get());
	Action->SetupPickUp(Target, MaxDistance);
	return Action;
}
