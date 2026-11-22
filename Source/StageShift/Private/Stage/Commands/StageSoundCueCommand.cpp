#include "Stage/Commands/StageSoundCueCommand.h"

#include "Physics/PhysicsActor.h"
#include "Stage/Actions/StageInteractionHelpers.h"
#include "Stage/Actions/StageSoundCueAction.h"
#include "Stage/Core/Data/StageDataAsset.h"

void UStageSoundCueCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (!Data || SoundControlId.IsNone() || !Data->TargetIds.Contains(SoundControlId))
		Errors.Add(Path + TEXT(" / Sound Control Id must be a declared target"));
	if (Data && !RequiredPropId.IsNone() && !Data->TargetIds.Contains(RequiredPropId))
		Errors.Add(Path + TEXT(" / Required Prop Id must be a declared target"));
	if (Data && !CueAnchorId.IsNone() && !Data->TargetIds.Contains(CueAnchorId))
		Errors.Add(Path + TEXT(" / Cue Anchor Id must be a declared target"));
	if (Data && !SoundTargetId.IsNone() && !Data->TargetIds.Contains(SoundTargetId))
		Errors.Add(Path + TEXT(" / Sound Target Id must be a declared target"));
	if (WindupAnimation && AnimationSlot.IsNone()) Errors.Add(Path + TEXT(" / Windup Animation requires an Animation Slot"));
}

void UStageSoundCueCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	const auto* Control = Cast<AStageSoundControl>(ResolveTarget(InContext, SoundControlId));
	if (!Control)
		Errors.Add(Path + TEXT(" / Sound Control Id must point to a StageSoundControl"));
	else if (!SoundId.IsNone() && !Control->HasSound(SoundId))
		Errors.Add(Path + TEXT(" / Sound Id must be configured on the selected console: ") + SoundId.ToString());
	if (!SoundTargetId.IsNone() && !ResolveTarget(InContext, SoundTargetId))
		Errors.Add(Path + TEXT(" / Sound Target Id must resolve to an actor"));
	if (!RequiredPropId.IsNone() && !Cast<AStagePhysicsActor>(ResolveTarget(InContext, RequiredPropId)))
		Errors.Add(Path + TEXT(" / Required Prop Id must point to a StagePhysicsActor"));
	if (!CueAnchorId.IsNone() && !ResolveTarget(InContext, CueAnchorId))
		Errors.Add(Path + TEXT(" / Cue Anchor Id must resolve to an actor"));
}

void UStageSoundCueCommand::OnPreStart_Implementation(const FStageExecutionContext& InContext)
{
	const auto* Control = Cast<AStageSoundControl>(ResolveTarget(InContext, SoundControlId));
	NeededTime = !SoundId.IsNone() ? Timing.GetDuration() : Control ? Control->GetWindowDuration() : 0.0f;
}

bool UStageSoundCueCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	if (Super::IsResultAchieved_Implementation(InContext)) return true;
	if (RequiredPropId.IsNone()) return false;
	const AActor* Owner = StageInteraction::GetOwner(InContext);
	const auto* Manipulator = IsValid(Owner) ? Owner->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	return !StageInteraction::IsHolding(Manipulator, ResolveTarget(InContext, RequiredPropId));
}

UStageAction* UStageSoundCueCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	auto* Member = InContext.MemberComponent.Get();
	auto* Control = Cast<AStageSoundControl>(ResolveTarget(InContext, SoundControlId));
	if (!Member || !Control) return nullptr;
	auto* Action = NewObject<UStageSoundCueAction>(Member);
	Action->Setup(Control, WindupAnimation, AnimationSlot, CueAnchorId, bRequireOnTime, FName(TEXT("StageSoundCue"), GetUniqueID()));
	if (!SoundId.IsNone()) Action->SetupSound(SoundId, ResolveTarget(InContext, SoundTargetId), Timing);
	Action->SetExecutionBudget(NeededTime + 1.0f);
	return Action;
}
