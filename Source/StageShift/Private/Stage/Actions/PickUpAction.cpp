#include "Stage/Actions/PickUpAction.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Physics/PhysicsActor.h"
#include "Core/StageShiftCharacter.h"
#include "Components/SceneComponent.h"
#include "Stage/Core/StageManager.h"
#include "StageInteractionHelpers.h"

void UPickUpAction::SetupPickUp(TArray<TWeakObjectPtr<AActor>> InCandidates, float InMaxDistance, TOptional<TRange<double>> InWindow,
    float InCheckDistanceTime, float InPickUpPeriodTime)
{
	Candidates = MoveTemp(InCandidates);
	MaxDistance = InMaxDistance;
	PickupWindow = InWindow;
	CheckDistanceTime = InCheckDistanceTime;
	PickUpPeriodTime = InPickUpPeriodTime;
}

void UPickUpAction::FailPickup(const FString& Details)
{
	const FString Message = FString::Printf(TEXT("PickUpAction / Owner[%s] / Target[%s] / Failed: %s"),
	    *GetNameSafe(StageInteraction::GetOwner(Context)), *GetNameSafe(Target.Get()), *Details);
	if (Context.Manager.IsValid()) Context.Manager->AddDiagnostic(Message);
	else UE_LOG(LogTemp, Warning, TEXT("%s"), *Message);

	const UWorld* World = GetWorld();
	if (!PickupWindow.IsSet() || !Manipulator.IsValid() || !World || World->GetTimeSeconds() >= PickupWindow->GetUpperBoundValue())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	NextPickupAttemptAt = World->GetTimeSeconds() + PickUpPeriodTime;
	CancelPickup();
	StopPickupMontage();
	Target.Reset();
}

void UPickUpAction::OnStart_Implementation()
{
	AActor* Owner = StageInteraction::GetOwner(Context);
	Manipulator = IsValid(Owner) ? Owner->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	if (!Manipulator.IsValid())
	{
		FailPickup(TEXT("Interactor has no valid PhysicsManipulationComponent"));
		return;
	}

	Manipulator->OnItemPickedUp.AddUniqueDynamic(this, &ThisClass::HandleItemPickedUp);
	if (PickupWindow.IsSet())
	{
		CheckForItem();
		return;
	}

	FInteractionContext Interaction;
	FString Failure = TEXT("No available Pick Up target");
	if (Candidates.Num() == 1)
	{
		Target = Candidates[0];
		if (StageInteraction::IsHolding(Manipulator.Get(), Target.Get()))
		{
			Finish(EStageActionResult::Succeeded);
			return;
		}
		if (BuildPickupContext(Target.Get(), Interaction, &Failure))
		{
			StartPickup(Interaction);
			return;
		}
	}
	else if (FindPickup(Interaction))
	{
		StartPickup(Interaction);
		return;
	}
	FailPickup(Failure);
}

void UPickUpAction::CheckForItem()
{
	UWorld* World = GetWorld();
	if (GetState() != EStageExecutionState::Running) return;
	if (!World || !Manipulator.IsValid())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	const double Now = World->GetTimeSeconds();
	const double UntilOpen = PickupWindow->GetLowerBoundValue() - Now;
	const double UntilClose = PickupWindow->GetUpperBoundValue() - Now;
	if (HasPendingCatch())
	{
		World->GetTimerManager().SetTimer(CheckTimer, this, &ThisClass::CheckForItem, CheckDistanceTime, false);
		return;
	}
	if (UntilClose <= 0.0)
	{
		FailPickup(TEXT("Pickup window expired"));
		return;
	}

	const float Delay = static_cast<float>(UntilOpen > 0.0 ? UntilOpen : FMath::Min(UntilClose, CheckDistanceTime));
	World->GetTimerManager().SetTimer(CheckTimer, this, &ThisClass::CheckForItem, Delay, false);
	if (UntilOpen > 0.0 || bOwnPickup || Now < NextPickupAttemptAt) return;

	FInteractionContext Interaction;
	if (FindPickup(Interaction)) StartPickup(Interaction);
}

bool UPickUpAction::HasPendingCatch() const
{
	if (!Manipulator.IsValid() || !Manipulator->IsCatchingProp()) return false;
	const auto* Component = Manipulator->GetManipulatedComponent();
	return Component && Candidates.Contains(TWeakObjectPtr<AActor>(Component->GetOwner()));
}

bool UPickUpAction::FindPickup(FInteractionContext& OutInteraction) const
{
	bool bFound = false;
	for (const TWeakObjectPtr<AActor>& Candidate : Candidates)
	{
		FInteractionContext Interaction;
		if (BuildPickupContext(Candidate.Get(), Interaction) && (!bFound || Interaction.Hit.Distance < OutInteraction.Hit.Distance))
		{
			OutInteraction = Interaction;
			bFound = true;
		}
	}
	return bFound;
}

bool UPickUpAction::BuildPickupContext(AActor* Item, FInteractionContext& OutInteraction, FString* OutFailure) const
{
	const auto Reject = [OutFailure](const FString& Reason)
	{
		if (OutFailure) *OutFailure = Reason;
		return false;
	};
	if (!IsValid(Item)) return Reject(TEXT("No available Pick Up target"));
	const auto* Prop = Cast<AStagePhysicsActor>(Item);
	if (!Prop || Prop->GetInteractionType() != EPhysicsInteractionType::Carry)
		return Reject(TEXT("Target must be a StagePhysicsActor with Carry interaction"));
	if (Prop->IsInCatchFlight()) return Reject(TEXT("Target is still in flight"));

	AActor* Owner = StageInteraction::GetOwner(Context);
	if (const AActor* Holder = StageInteraction::GetOtherItemHolder(Owner, Item))
		return Reject(FString::Printf(TEXT("Target is held by [%s]"), *Holder->GetActorNameOrLabel()));
	if (!StageInteraction::MakeContext(Owner, Item, FGameplayTag(), EInteractionPhase::Start, MaxDistance, OutInteraction, OutFailure)) return false;
	if (IInteractionInterface::Execute_CanInteract(Item, OutInteraction)) return true;
	if (OutFailure && Manipulator.IsValid())
	{
		const auto* Component = OutInteraction.Hit.GetComponent();
		const auto* CarryPoint = Manipulator->GetCarryPoint();
		const auto* Character = Cast<AStageShiftCharacter>(Owner);
		*OutFailure = FString::Printf(
		    TEXT("CanInteract rejected: CanManipulate=%d, CarryPoint=%s, CarryPointOwner=%s, HitComponent=%s, Registered=%d, SimulatingPhysics=%d, IsRoot=%d, Manipulated=%s, Grabbed=%s, PhysicsMontageActive=%d"),
		    Manipulator->CanManipulate(OutInteraction.Hit, EPhysicsInteractionType::Carry), *GetNameSafe(CarryPoint), *GetNameSafe(CarryPoint ? CarryPoint->GetOwner() : nullptr),
		    *GetNameSafe(Component), Component && Component->IsRegistered(), Component && Component->IsSimulatingPhysics(OutInteraction.Hit.BoneName),
		    Component && Component == Item->GetRootComponent(), *GetNameSafe(Manipulator->GetManipulatedComponent()), *GetNameSafe(Manipulator->GetGrabbedComponent()),
		    Character && Character->IsPhysicsMontageActive());
	}
	return false;
}

void UPickUpAction::StartPickup(const FInteractionContext& Interaction)
{
	AActor* Item = Interaction.Hit.GetActor();
	if (GetState() != EStageExecutionState::Running || !IsValid(Item) || !Manipulator.IsValid()) return;
	AActor* Owner = StageInteraction::GetOwner(Context);
	Target = Item;
	InteractionHit = Interaction.Hit;
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
		FailPickup(FString::Printf(TEXT("OnInteraction rejected pickup after CanInteract passed: Manipulated=%s, CarryPoint=%s"),
		    *GetNameSafe(Manipulator->GetManipulatedComponent()), *GetNameSafe(Manipulator->GetCarryPoint())));
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
		FailPickup(FString::Printf(TEXT("Item not attached and pickup animation unavailable: InteractionResult=%s, AnimationConfigured=%d, Montage=%s, AnimInstance=%s"),
		    *UEnum::GetValueAsString(Result), Animation != nullptr, *GetNameSafe(Animation ? Animation->Montage.Get() : nullptr), *GetNameSafe(Instance)));
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
	{
		FailPickup(FString::Printf(TEXT("Pickup montage did not start or is no longer active: Montage=%s, StartSection=%s, PlayResult=%.3f, Active=%d"),
		    *GetNameSafe(MontageToPlay), *StartSection.ToString(), Duration, Instance->Montage_IsActive(MontageToPlay)));
	}
}

void UPickUpAction::HandleItemPickedUp(AActor* Item)
{
	if (bStarting || !StageInteraction::IsHolding(Manipulator.Get(), Item)) return;
	if (PickupWindow.IsSet() ? Candidates.Contains(TWeakObjectPtr<AActor>(Item)) : Item == Target.Get()) Finish(EStageActionResult::Succeeded);
}

void UPickUpAction::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bStarting || Montage != PlayingMontage.Get()) return;
	if (PlaybackInstance.IsValid() && PlaybackInstance->Montage_IsActive(Montage)) return;
	if (StageInteraction::IsHolding(Manipulator.Get(), Target.Get())) Finish(EStageActionResult::Succeeded);
	else FailPickup(FString::Printf(TEXT("Pickup montage ended without the item in hand: Montage=%s, Interrupted=%d, Manipulated=%s. Check GrabObject notify and hand reach."),
	    *GetNameSafe(Montage), bInterrupted, *GetNameSafe(Manipulator.IsValid() ? Manipulator->GetManipulatedComponent() : nullptr)));
}

void UPickUpAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (Mode == EStageActionStopMode::StopAction) Finish(EStageActionResult::Interrupted);
}

void UPickUpAction::CancelPickup()
{
	if (HasPendingCatch())
	{
		Manipulator->ReleaseComponent();
		return;
	}
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
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(CheckTimer);
	if (Manipulator.IsValid()) Manipulator->OnItemPickedUp.RemoveDynamic(this, &ThisClass::HandleItemPickedUp);
	if (GetResult() != EStageActionResult::Succeeded) CancelPickup();
	if (!bStarting) StopPickupMontage();
}
