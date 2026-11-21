#include "Stage/Actions/InteractAction.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Physics/PhysicsActor.h"
#include "Stage/Core/StageManager.h"
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
	const auto FailInteraction = [this](const FString& Reason)
	{
		const FString Message = FString::Printf(TEXT("InteractAction / Target[%s] / Failed: %s"), *GetNameSafe(Target.Get()), *Reason);
		if (Context.Manager.IsValid()) Context.Manager->AddDiagnostic(Message);
		else UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);
		Finish(EStageActionResult::Failed);
	};

	FInteractionContext Interaction;
	AActor* Owner = StageInteraction::GetOwner(Context);
	const auto* Item = Cast<AStagePhysicsActor>(Target.Get());
	if (!FMath::IsFinite(HoldDuration) || HoldDuration < 0.0f || (InteractionPhase != EInteractionPhase::Start && InteractionPhase != EInteractionPhase::End))
	{
		FailInteraction(TEXT("Invalid phase or hold duration"));
		return;
	}
	if (!Target.IsValid())
	{
		FailInteraction(TEXT("No available Interact target: no matching held item or reachable interaction target"));
		return;
	}
	if (Item && Item->GetInteractionType() == EPhysicsInteractionType::Carry)
	{
		const auto* Manipulator = IsValid(Owner) ? Owner->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
		if (!StageInteraction::IsHolding(Manipulator, Item))
		{
			FailInteraction(TEXT("The Interact target is no longer held by this participant"));
			return;
		}
	}
	FString ContextFailure;
	if (!StageInteraction::MakeContext(Owner, Target.Get(), ActionTag, InteractionPhase, MaxDistance, Interaction, &ContextFailure))
	{
		FailInteraction(ContextFailure);
		return;
	}
	// End must also work when CanInteract rejects an already held item.
	if (InteractionPhase == EInteractionPhase::Start && !IInteractionInterface::Execute_CanInteract(Target.Get(), Interaction))
	{
		FailInteraction(TEXT("CanInteract returned false"));
		return;
	}
	if (GetState() == EStageExecutionState::Finished) return;
	if (!Target.IsValid())
	{
		FailInteraction(TEXT("Target became invalid"));
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
	if (InteractionPhase == EInteractionPhase::Start && Result != EInteractionResult::Rejected)
		if (auto* Partner = Target.IsValid() ? Target->FindComponentByClass<UStageMemberComponent>() : nullptr)
		{
			Partner->ShowPartnerInteractionPose();
			if (auto* Member = Context.MemberComponent.Get()) Member->ShowPartnerInteractionPose();
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
	else FailInteraction(FString::Printf(TEXT("OnInteraction returned %s"), *UEnum::GetValueAsString(Result)));
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
