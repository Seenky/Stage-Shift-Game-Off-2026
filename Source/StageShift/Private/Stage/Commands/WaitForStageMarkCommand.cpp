#include "Stage/Commands/WaitForStageMarkCommand.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Actions/WaitForStageMarkAction.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"

void UWaitForStageMarkCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (TargetRoleId.IsNone() || !Data || !Data->GetRoleOptions().Contains(TargetRoleId)) Errors.Add(Path + TEXT(" / TargetRoleId must be a declared role"));
}

bool UWaitForStageMarkCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	const AActor* Actor = InContext.Manager.IsValid() ? InContext.Manager->GetParticipant(TargetRoleId) : nullptr;
	const auto* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
	return IsValid(Member) && !Member->IsPhysicallyBlocked() && Member->IsAtStageMark();
}

UStageAction* UWaitForStageMarkCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.IsValid() || !InContext.Manager.IsValid()) return nullptr;
	AActor* Actor = InContext.Manager->GetParticipant(TargetRoleId);
	auto* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
	if (!IsValid(Member)) return nullptr;

	auto* Action = NewObject<UWaitForStageMarkAction>(InContext.MemberComponent.Get());
	Action->SetupWaitForStageMark(Member);
	// Allow the stage/finale deadline to control the wait, as in WaitForStatusCommand.
	if (const UStageDataAsset* Stage = InContext.Manager->GetActiveStage()) Action->SetExecutionBudget(Stage->MainDuration + Stage->FinaleBudget);
	return Action;
}
