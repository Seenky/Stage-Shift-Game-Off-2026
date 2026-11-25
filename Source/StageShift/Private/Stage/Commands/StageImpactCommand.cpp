#include "Stage/Commands/StageImpactCommand.h"

#include "Components/Physics/PhysicalMontageComponent.h"
#include "Physics/PhysicsActor.h"
#include "Stage/Actions/StageInteractionHelpers.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Environment/StageSoundControl.h"

AActor* UStageImpactCommand::ResolvePerformingTarget(const FStageExecutionContext& InContext) const
{
	if (Mode == EStageImpactMode::Collapse) return StageInteraction::GetOwner(InContext);
	const auto* Manager = InContext.Manager.Get();
	return Manager ? Manager->GetParticipant(TargetRoleId) : nullptr;
}

float UStageImpactCommand::GetNeededTime_Implementation() const
{
	return ImpactDelay + (Mode == EStageImpactMode::Throw ? 1.3f : 0.25f);
}

bool UStageImpactCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	const AActor* Target = ResolvePerformingTarget(InContext);
	const auto* Physics = IsValid(Target) ? Target->FindComponentByClass<UPhysicalMontageComponent>() : nullptr;
	return Super::IsResultAchieved_Implementation(InContext) || (Physics && Physics->IsStayingDown());
}

UStageAction* UStageImpactCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	auto* Member = InContext.MemberComponent.Get();
	AActor* Target = ResolvePerformingTarget(InContext);
	if (!Member || !IsValid(Target) || !Target->FindComponentByClass<UPhysicalMontageComponent>()) return nullptr;
	if (Mode != EStageImpactMode::Collapse && Target == Member->GetOwner()) return nullptr;
	AStageSoundControl* SoundControl = nullptr;
	if (Mode != EStageImpactMode::Collapse && !SyncWithSoundControlId.IsNone())
	{
		SoundControl = Cast<AStageSoundControl>(ResolveTarget(InContext, SyncWithSoundControlId));
		if (!SoundControl) return nullptr;
	}
	auto* Action = NewObject<UStageImpactAction>(Member);
	Action->Setup(Mode, Target, Cast<AStagePhysicsActor>(ResolveTarget(InContext, PropId)),
	    ImpactDelay, StageTimeRequired, WindupAnimation, ThrowAnimation, FallMontage, SoundControl);
	Action->SetExecutionBudget(GetNeededTime() + 2.0f);
	return Action;
}

void UStageImpactCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	if (Mode != EStageImpactMode::Collapse && (!Data || !Data->GetRoleOptions().Contains(TargetRoleId) || !Data->TargetIds.Contains(PropId)))
		Errors.Add(Path + TEXT(" / Stage Impact needs a declared target role and prop"));
	if (Mode != EStageImpactMode::Collapse && !SyncWithSoundControlId.IsNone() && (!Data || !Data->TargetIds.Contains(SyncWithSoundControlId)))
		Errors.Add(Path + TEXT(" / Sync with Sound Control Id must be a declared target"));
	if (!FMath::IsFinite(ImpactDelay) || ImpactDelay < 0.0f || !FMath::IsFinite(StageTimeRequired) || StageTimeRequired < 0.0f)
		Errors.Add(Path + TEXT(" / Stage Impact timings must be finite and nonnegative"));
}

void UStageImpactCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	if (Mode == EStageImpactMode::Collapse)
	{
		const AActor* Owner = StageInteraction::GetOwner(InContext);
		if (!IsValid(Owner) || !Owner->FindComponentByClass<UPhysicalMontageComponent>())
			Errors.Add(Path + TEXT(" / Stage Impact collapse needs a performer with a PhysicalMontageComponent"));
	}
	else if (!Cast<AStagePhysicsActor>(ResolveTarget(InContext, PropId)))
		Errors.Add(Path + TEXT(" / Stage Impact prop must be a StagePhysicsActor"));
	if (Mode != EStageImpactMode::Collapse && !SyncWithSoundControlId.IsNone()
	    && !Cast<AStageSoundControl>(ResolveTarget(InContext, SyncWithSoundControlId)))
		Errors.Add(Path + TEXT(" / Sync with Sound Control Id must point to a StageSoundControl"));
}
