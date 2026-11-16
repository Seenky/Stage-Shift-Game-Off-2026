#include "Stage/Actions/PickUpAction.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Physics/PhysicsActor.h"
#include "StageInteractionHelpers.h"

void UPickUpAction::SetupPickUp(AActor* Item, float InMaxDistance)
{
	Target = Item;
	MaxDistance = InMaxDistance;
}

void UPickUpAction::OnStart_Implementation()
{
	AActor* Owner = StageInteraction::GetOwner(Context);
	auto* Item = Cast<AStagePhysicsActor>(Target.Get());
	Manipulator = IsValid(Owner) ? Owner->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	if (!Manipulator.IsValid() || !Item || Item->GetInteractionType() != EPhysicsInteractionType::Carry)
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	if (StageInteraction::IsHolding(Manipulator.Get(), Item))
	{
		Finish(EStageActionResult::Succeeded);
		return;
	}
	FInteractionContext Interaction;
	if (!StageInteraction::MakeContext(Owner, Item, FGameplayTag(), EInteractionPhase::Start, MaxDistance, Interaction)
	    || !IInteractionInterface::Execute_CanInteract(Item, Interaction))
	{
		Finish(EStageActionResult::Failed);
		return;
	}
	if (GetState() == EStageExecutionState::Finished) return;
	InteractionHit = Interaction.Hit;
	Manipulator->OnItemPickedUp.AddUniqueDynamic(this, &ThisClass::HandleItemPickedUp);
	// Pickup can finish synchronously before the target has recorded its manipulator.
	bStarting = true;
	const EInteractionResult Result = IInteractionInterface::Execute_OnInteraction(Item, Interaction);
	bOwnPickup = Result == EInteractionResult::Active || Result == EInteractionResult::Latched;
	if (GetState() == EStageExecutionState::Finished)
	{
		bStarting = false;
		CancelPickup();
		return;
	}
	if (Result == EInteractionResult::Rejected)
	{
		bStarting = false;
		Finish(EStageActionResult::Failed);
		return;
	}
	if (StageInteraction::IsHolding(Manipulator.Get(), Target.Get()))
	{
		bStarting = false;
		Finish(EStageActionResult::Succeeded);
		return;
	}

	const auto* Animation = StageInteraction::FindAnimation(Owner, Target.Get());
	const auto* Character = Cast<ACharacter>(Owner);
	UAnimInstance* Instance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!bOwnPickup || !Animation || !Animation->Montage || !Instance)
	{
		bStarting = false;
		Finish(EStageActionResult::Failed);
		return;
	}
	UAnimMontage* MontageToPlay = Animation->Montage.Get();
	const FName StartSection = Animation->StartSection;
	PlayingMontage = MontageToPlay;
	PlaybackInstance = Instance;
	Instance->OnMontageEnded.AddUniqueDynamic(this, &ThisClass::HandleMontageEnded);
	const float Duration = Instance->Montage_Play(MontageToPlay, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
	if (Duration > 0.0f)
	{
		if (GetState() != EStageExecutionState::Finished && !StartSection.IsNone()) Instance->Montage_JumpToSection(StartSection, MontageToPlay);
		if (const auto* Montage = Instance->GetActiveInstanceForMontage(MontageToPlay)) MontageInstanceId = Montage->GetInstanceID();
	}
	bStarting = false;
	if (GetState() == EStageExecutionState::Finished)
	{
		CancelPickup();
		StopPickupMontage();
		return;
	}
	if (StageInteraction::IsHolding(Manipulator.Get(), Target.Get()))
		Finish(EStageActionResult::Succeeded);
	else if (Duration <= 0.0f || !Instance->Montage_IsActive(PlayingMontage.Get()))
		Finish(EStageActionResult::Failed);
}

void UPickUpAction::HandleItemPickedUp(AActor* Item)
{
	if (!bStarting && Item == Target.Get() && StageInteraction::IsHolding(Manipulator.Get(), Item)) Finish(EStageActionResult::Succeeded);
}

void UPickUpAction::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bStarting || Montage != PlayingMontage.Get()) return;
	if (PlaybackInstance.IsValid() && PlaybackInstance->Montage_IsActive(Montage)) return;
	Finish(StageInteraction::IsHolding(Manipulator.Get(), Target.Get()) ? EStageActionResult::Succeeded : EStageActionResult::Failed);
}

void UPickUpAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (Mode == EStageActionStopMode::StopAction) Finish(EStageActionResult::Interrupted);
}

void UPickUpAction::CancelPickup()
{
	if (!bOwnPickup) return;
	bOwnPickup = false;
	const auto* Component = Manipulator.IsValid() ? Manipulator->GetManipulatedComponent() : nullptr;
	if (!Target.IsValid() || !Component || Component->GetOwner() != Target.Get()) return;
	FInteractionContext Interaction;
	Interaction.Interactor = StageInteraction::GetOwner(Context);
	Interaction.Hit = InteractionHit;
	Interaction.InteractionPhase = EInteractionPhase::End;
	Interaction.bCancelled = true;
	IInteractionInterface::Execute_OnInteraction(Target.Get(), Interaction);
}

void UPickUpAction::StopPickupMontage()
{
	UAnimInstance* Instance = PlaybackInstance.Get();
	UAnimMontage* Montage = PlayingMontage.Get();
	const int32 InstanceId = MontageInstanceId;
	PlaybackInstance.Reset();
	PlayingMontage.Reset();
	MontageInstanceId = INDEX_NONE;
	if (!Instance) return;
	Instance->OnMontageEnded.RemoveDynamic(this, &ThisClass::HandleMontageEnded);
	const auto* Active = Montage ? Instance->GetActiveInstanceForMontage(Montage) : nullptr;
	if (Active && Active->GetInstanceID() == InstanceId) Instance->Montage_Stop(0.08f, Montage);
}

void UPickUpAction::Cleanup_Implementation()
{
	if (Manipulator.IsValid()) Manipulator->OnItemPickedUp.RemoveDynamic(this, &ThisClass::HandleItemPickedUp);
	if (GetResult() != EStageActionResult::Succeeded) CancelPickup();
	if (!bStarting) StopPickupMontage();
}
