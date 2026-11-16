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

void UPickUpCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (TargetId.IsNone() || !Data || !Data->TargetIds.Contains(TargetId)) Errors.Add(Path + TEXT(" / TargetId must be a declared target"));
	if (!FMath::IsFinite(MaxDistance) || MaxDistance <= 0) Errors.Add(Path + TEXT(" / MaxDistance must be positive and finite"));
}

void UPickUpCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	const auto* Item = Cast<AStagePhysicsActor>(ResolveTarget(InContext, TargetId));
	if (!Item || Item->GetInteractionType() != EPhysicsInteractionType::Carry) Errors.Add(Path + TEXT(" / PickUp requires a StagePhysicsActor with Carry interaction"));
	const AActor* Owner = StageInteraction::GetOwner(InContext);
	if (IsValid(Owner) && !Owner->FindComponentByClass<UPhysicsManipulationComponent>()) Errors.Add(Path + TEXT(" / PickUp requires PhysicsManipulationComponent and a configured CarryPoint"));
}

void UPickUpCommand::OnPreStart_Implementation(const FStageExecutionContext& InContext)
{
	NeededTime = 0.0f;
	const auto* Animation = StageInteraction::FindAnimation(StageInteraction::GetOwner(InContext), ResolveTarget(InContext, TargetId));
	if (!Animation || !Animation->Montage) return;
	const UAnimMontage* Montage = Animation->Montage;
	if (!Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event) { return Event.Notify && Event.Notify->GetNotifyName() == TEXT("GrabObject"); })) return;
	const int32 Section = Montage->GetSectionIndex(Animation->StartSection);
	NeededTime = Section == INDEX_NONE ? Montage->GetPlayLength() : Montage->GetSectionLength(Section);
}

float UPickUpCommand::GetNeededTime_Implementation() const
{
	return NeededTime;
}

bool UPickUpCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	const AActor* Owner = StageInteraction::GetOwner(InContext);
	return StageInteraction::IsHolding(IsValid(Owner) ? Owner->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr, ResolveTarget(InContext, TargetId));
}

UStageAction* UPickUpCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.IsValid() || !InContext.Manager.IsValid()) return nullptr;
	auto* Action = NewObject<UPickUpAction>(InContext.MemberComponent.Get());
	Action->SetupPickUp(ResolveTarget(InContext, TargetId), MaxDistance);
	return Action;
}
