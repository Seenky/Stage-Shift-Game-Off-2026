#include "Stage/Commands/StageSpotlightCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Actions/StageSpotlightAction.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"

float UStageSpotlightCommand::GetNeededTime_Implementation() const
{
	return TimeLimit;
}

void UStageSpotlightCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (SpotlightId.IsNone() || !Data || !Data->TargetIds.Contains(SpotlightId))
		Errors.Add(Path + TEXT(" / Spotlight Id must be a declared target"));
	if (TargetRoleId.IsNone() || !Data || !Data->GetRoleOptions().Contains(TargetRoleId))
		Errors.Add(Path + TEXT(" / Target Role Id must be a declared role"));
	if (!FMath::IsFinite(TimeLimit) || TimeLimit < 0.1f)
		Errors.Add(Path + TEXT(" / Spotlight Time Limit must be at least 0.1 seconds"));
}

void UStageSpotlightCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	if (!Cast<AStageSpotlightControl>(ResolveTarget(InContext, SpotlightId)))
		Errors.Add(Path + TEXT(" / Spotlight Id must point to a StageSpotlightControl"));
}

UStageAction* UStageSpotlightCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	auto* Manager = InContext.Manager.Get();
	auto* Member = InContext.MemberComponent.Get();
	auto* Spotlight = Cast<AStageSpotlightControl>(ResolveTarget(InContext, SpotlightId));
	AActor* FocusActor = Manager ? Manager->GetParticipant(TargetRoleId) : nullptr;
	if (!Member || !Spotlight || !IsValid(FocusActor) || FocusActor->IsActorBeingDestroyed() ||
	    !FMath::IsFinite(TimeLimit) || TimeLimit < 0.1f) return nullptr;

	auto* Action = NewObject<UStageSpotlightAction>(Member);
	Action->Setup(Spotlight, FocusActor, TargetRoleId, TargetDisplayName.IsEmpty() ? FText::FromName(TargetRoleId) : TargetDisplayName, TimeLimit);
	Action->SetExecutionBudget(TimeLimit + 1.0f);
	return Action;
}
