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

void UInteractCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (TargetId.IsNone() || !Data || !Data->TargetIds.Contains(TargetId)) Errors.Add(Path + TEXT(" / TargetId must be a declared target"));
	if (InteractionPhase != EInteractionPhase::Start && InteractionPhase != EInteractionPhase::End) Errors.Add(Path + TEXT(" / invalid interaction phase"));
	if (!FMath::IsFinite(HoldDuration) || HoldDuration < 0) Errors.Add(Path + TEXT(" / HoldDuration must be nonnegative and finite"));
	if (!FMath::IsFinite(MaxDistance) || MaxDistance <= 0) Errors.Add(Path + TEXT(" / MaxDistance must be positive and finite"));
}

void UInteractCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	const AActor* Target = ResolveTarget(InContext, TargetId);
	if (!IsValid(Target) || !Target->Implements<UInteractionInterface>()) Errors.Add(Path + TEXT(" / target must implement InteractionInterface"));
	const auto* Item = Cast<AStagePhysicsActor>(Target);
	if (InteractionPhase == EInteractionPhase::Start && Item && Item->GetInteractionType() == EPhysicsInteractionType::Carry)
		Errors.Add(Path + TEXT(" / use PickUpCommand to start carrying an item; Interact End can release it"));
}

float UInteractCommand::GetNeededTime_Implementation() const
{
	return InteractionPhase == EInteractionPhase::Start ? HoldDuration : 0.0f;
}

UStageAction* UInteractCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.IsValid() || !InContext.Manager.IsValid()) return nullptr;
	auto* Action = NewObject<UInteractAction>(InContext.MemberComponent.Get());
	Action->SetupInteraction(ResolveTarget(InContext, TargetId), ActionTag, InteractionPhase, HoldDuration, MaxDistance);
	return Action;
}
