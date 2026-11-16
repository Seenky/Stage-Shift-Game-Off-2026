#include "Stage/Actions/InteractAction.h"

#include "Physics/PhysicsActor.h"
#include "StageInteractionHelpers.h"
#include "TimerManager.h"

void UInteractAction::SetupInteraction(AActor* TargetActor, FGameplayTag InActionTag, EInteractionPhase InPhase, float InHoldDuration, float InMaxDistance)
{
	Target = TargetActor;
	ActionTag = InActionTag;
	InteractionPhase = InPhase;
	HoldDuration = InHoldDuration;
	MaxDistance = InMaxDistance;
}

void UInteractAction::OnStart_Implementation()
{
	FInteractionContext Interaction;
	const auto* Item = Cast<AStagePhysicsActor>(Target.Get());
	if (!FMath::IsFinite(HoldDuration) || HoldDuration < 0.0f
	    || (InteractionPhase != EInteractionPhase::Start && InteractionPhase != EInteractionPhase::End)
	    || (InteractionPhase == EInteractionPhase::Start && Item && Item->GetInteractionType() == EPhysicsInteractionType::Carry)
	    || !StageInteraction::MakeContext(StageInteraction::GetOwner(Context), Target.Get(), ActionTag, InteractionPhase, MaxDistance, Interaction))
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	// End must also work when CanInteract rejects an already held item.
	if (InteractionPhase == EInteractionPhase::Start && !IInteractionInterface::Execute_CanInteract(Target.Get(), Interaction))
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	if (GetState() == EStageExecutionState::Finished) return;
	if (!Target.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	InteractionHit = Interaction.Hit;
	StartedAt = GetWorld()->GetTimeSeconds();
	if (InteractionPhase == EInteractionPhase::End) Interaction.HoldDuration = HoldDuration;
	const EInteractionResult Result = IInteractionInterface::Execute_OnInteraction(Target.Get(), Interaction);
	bInteractionActive = InteractionPhase == EInteractionPhase::Start && (Result == EInteractionResult::Active || Result == EInteractionResult::Latched);
	if (GetState() == EStageExecutionState::Finished)
	{
		if (bInteractionActive) SendEnd(true);
		return;
	}
	if (Result == EInteractionResult::Completed)
	{
		Finish(EStageActionResult::Succeeded);
	}
	else if (InteractionPhase == EInteractionPhase::Start && Result == EInteractionResult::Latched)
	{
		// A successful latch persists after this Action; a later Interact End releases it.
		bInteractionActive = false;
		Finish(EStageActionResult::Succeeded);
	}
	else if (InteractionPhase == EInteractionPhase::Start && Result == EInteractionResult::Active)
	{
		if (HoldDuration > 0.0f) GetWorld()->GetTimerManager().SetTimer(HoldTimer, this, &ThisClass::EndInteraction, HoldDuration, false);
		else EndInteraction();
	}
	else Finish(EStageActionResult::Failed);
}

EInteractionResult UInteractAction::SendEnd(bool bCancelled)
{
	bInteractionActive = false;
	FInteractionContext Interaction;
	if (!StageInteraction::MakeContext(StageInteraction::GetOwner(Context), Target.Get(), ActionTag, EInteractionPhase::End, MaxDistance, Interaction))
		return EInteractionResult::Rejected;
	Interaction.Hit = InteractionHit;
	Interaction.HoldDuration = GetWorld() ? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - StartedAt) : 0.0;
	Interaction.bCancelled = bCancelled;
	return IInteractionInterface::Execute_OnInteraction(Target.Get(), Interaction);
}

void UInteractAction::EndInteraction()
{
	const EInteractionResult Result = SendEnd(false);
	Finish(Result == EInteractionResult::Completed ? EStageActionResult::Succeeded : EStageActionResult::Failed);
}

void UInteractAction::Cleanup_Implementation()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HoldTimer);
	if (bInteractionActive) SendEnd(true);
}
