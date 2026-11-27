// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Physics/PhysicsActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"
#include "Stage/Environment/StageCurtainRail.h"
#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"

AStagePhysicsActor::AStagePhysicsActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	GetStaticMeshComponent()->SetCollisionProfileName(TEXT("PhysicsActor"));
	ConfigureCharacterContact();
	GetStaticMeshComponent()->SetSimulatePhysics(true);
	SetMobility(EComponentMobility::Movable);
}

void AStagePhysicsActor::BeginPlay()
{
	ConfigureCharacterContact();
	Super::BeginPlay();
	RefreshPickupOutline();
	if (ImpactSound)
	{
		GetStaticMeshComponent()->SetNotifyRigidBodyCollision(true);
		GetStaticMeshComponent()->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleImpactSound);
	}
}

void AStagePhysicsActor::RefreshPickupOutline()
{
	UStaticMeshComponent* Mesh = GetStaticMeshComponent();
	if (InteractionType == EPhysicsInteractionType::None || IsActorBeingDestroyed() || GetNetMode() == NM_DedicatedServer)
	{
		PickupOutline.Reset();
		bPickupOutlineFocused = false;
		return;
	}

	const bool bAvailable = InteractionType == EPhysicsInteractionType::Carry && Mesh->IsSimulatingPhysics() && Mesh->IsQueryCollisionEnabled();
	if (!bAvailable) bPickupOutlineFocused = false;
	PickupOutline.Apply(*Mesh, bAvailable ? GetPickupOutlineMask(bPickupOutlineFocused) : FInteractionOutlineStencil::PlayerOcclusionBlocker);
}

int32 AStagePhysicsActor::GetPickupOutlineMask(bool bFocused) const
{
	return FInteractionOutlineStencil::PlayerOcclusionBlocker | FInteractionOutlineStencil::Available | (bFocused ? FInteractionOutlineStencil::Focused : 0)
	    | (PickupCueHighlightSources.IsEmpty() ? 0 : FInteractionOutlineStencil::PickupCue);
}

bool AStagePhysicsActor::SetPickupOutlineFocused(bool bFocused)
{
	const auto* Mesh = GetStaticMeshComponent();
	if (InteractionType != EPhysicsInteractionType::Carry || !Mesh->IsSimulatingPhysics() || !Mesh->IsQueryCollisionEnabled()) return false;
	const bool bUpdated = PickupOutline.Update(GetPickupOutlineMask(bFocused));
	bPickupOutlineFocused = bUpdated && bFocused;
	return bUpdated;
}

void AStagePhysicsActor::SetPickupCueHighlighted(UObject& Source, bool bHighlighted)
{
	if (bHighlighted) PickupCueHighlightSources.Add(&Source);
	else PickupCueHighlightSources.Remove(&Source);
	for (auto It = PickupCueHighlightSources.CreateIterator(); It; ++It)
		if (!It->IsValid()) It.RemoveCurrent();
	RefreshPickupOutline();
}

void AStagePhysicsActor::HandleImpactSound(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
    FVector NormalImpulse, const FHitResult& Hit)
{
	if (!ImpactSound || !HitComponent || !HitComponent->IsSimulatingPhysics() || !GetWorld() || GetGameTimeSinceCreation() < .16f) return;

	const double ImpactSpeed = NormalImpulse.Size() / FMath::Max(HitComponent->GetMass(), .1f);
	const double Now = GetWorld()->GetTimeSeconds();
	if (ImpactSpeed < 83.0 || Now - LastImpactSoundTime < .16) return;

	LastImpactSoundTime = Now;
	const float Volume = FMath::Clamp(static_cast<float>(ImpactSpeed / 520.0), .25f, .92f);
	UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Hit.ImpactPoint, Volume, FMath::FRandRange(.91f, 1.12f));
}

void AStagePhysicsActor::ConfigureCharacterContact()
{
	UStaticMeshComponent* MeshComponent = GetStaticMeshComponent();
	MeshComponent->CanCharacterStepUpOn = ECB_No;
	MeshComponent->SetWalkableSlopeOverride(FWalkableSlopeOverride(WalkableSlope_Unwalkable, 0.0f));
}

bool AStagePhysicsActor::CanInteract_Implementation(const FInteractionContext& InteractionContext) const
{
	if (const auto* Rail = CurtainRail.Get(); Rail && !Rail->CanDragLever()) return false;
	if (!InteractionContext.Interactor || InteractionContext.Hit.GetComponent() != GetStaticMeshComponent()
	    || IsBeingManipulated())
	{
		return false;
	}
	const UPhysicsManipulationComponent* Manipulator = InteractionContext.Interactor->FindComponentByClass<UPhysicsManipulationComponent>();
	return Manipulator && Manipulator->CanManipulate(InteractionContext.Hit, InteractionType);
}

void AStagePhysicsActor::SetCurtainRail(AStageCurtainRail* Rail)
{
	CurtainRail = Rail;
	InteractionType = EPhysicsInteractionType::Drag;
	RefreshPickupOutline();
}

const UMeshComponent* AStagePhysicsActor::GetInteractionMesh() const
{
	const AStageCurtainRail* Rail = CurtainRail.Get();
	return Rail ? Rail->GetControlVisual() : nullptr;
}

bool AStagePhysicsActor::IsBeingDragged() const
{
	const auto* Manipulator = ActiveManipulator.Get();
	return Manipulator && Manipulator->IsDragging() && Manipulator->GetManipulatedComponent() == GetStaticMeshComponent();
}

bool AStagePhysicsActor::IsBeingManipulated() const
{
	const auto* Manipulator = ActiveManipulator.Get();
	return Manipulator && Manipulator->GetManipulatedComponent() == GetStaticMeshComponent();
}

bool AStagePhysicsActor::HasClearReach(FVector From, FVector To, const AActor* Source, const AActor* Receiver) const
{
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StagePropReach), false, this);
	if (Source) Query.AddIgnoredActor(Source);
	if (Receiver) Query.AddIgnoredActor(Receiver);
	return GetWorld() && !GetWorld()->LineTraceTestByChannel(From, To, ECC_Visibility, Query);
}

bool AStagePhysicsActor::CanHandOff(const AActor* From, const AActor* To) const
{
	if (InteractionType != EPhysicsInteractionType::Carry || !IsValid(From) || !IsValid(To) || From == To || To->GetWorld() != GetWorld()
	    || FVector::DistSquared(From->GetActorLocation(), To->GetActorLocation()) > FMath::Square(HandoffRange)) return false;
	const auto* Sender = From->FindComponentByClass<UPhysicsManipulationComponent>();
	const auto* Receiver = To->FindComponentByClass<UPhysicsManipulationComponent>();
	return Sender && Sender->IsCarrying() && Sender->GetManipulatedComponent() == GetStaticMeshComponent() && Receiver && Receiver->CanReceiveStageProp()
	    && HasClearReach(From->GetActorLocation(), To->GetActorLocation(), From, To)
	    && HasClearReach(GetActorLocation(), Receiver->GetCarryPoint()->GetComponentLocation(), From, To);
}

bool AStagePhysicsActor::TryHandOff(AActor* From, AActor* To)
{
	if (!CanHandOff(From, To)) return false;
	auto* Sender = From->FindComponentByClass<UPhysicsManipulationComponent>();
	auto* Receiver = To->FindComponentByClass<UPhysicsManipulationComponent>();
	const FVector Contact = Sender->GetCarryPoint()->GetComponentLocation();
	ActiveManipulator = Receiver;
	if (!Sender->TransferCarriedPropTo(*Receiver))
	{
		ActiveManipulator = Sender;
		return false;
	}
	if (auto* Member = IsValid(From) ? From->FindComponentByClass<UStageMemberComponent>() : nullptr) Member->ShowPartnerInteractionPose();
	if (auto* Member = IsValid(To) ? To->FindComponentByClass<UStageMemberComponent>() : nullptr) Member->ShowPartnerInteractionPose();
	if (auto* Interaction = From->FindComponentByClass<UInteractionComponent>())
		Interaction->PlayHandMotion(EInteractionHandMotion::Give, Contact, From->GetRootComponent());
	if (auto* Interaction = To->FindComponentByClass<UInteractionComponent>())
		Interaction->PlayHandMotion(EInteractionHandMotion::Receive, Contact, To->GetRootComponent());
	Receiver->OnItemPickedUp.Broadcast(this);
	OnPropTransfer.Broadcast(this, From, To, EStagePropTransferResult::HandedOff, 0.f);
	return true;
}

void AStagePhysicsActor::WatchThrownProp(AActor* Source)
{
	if (InteractionType != EPhysicsInteractionType::Carry || !IsValid(Source) || !GetWorld() || !GetStaticMeshComponent()->IsSimulatingPhysics()) return;
	ActiveManipulator.Reset();
	Thrower = Source;
	IgnoreThrowerDuringRelease(*Source);
	PreviousFlightPosition = GetStaticMeshComponent()->GetCenterOfMass();
	CatchDeadline = GetWorld()->GetTimeSeconds() + CatchWindow;
	bMissedThrow = false;
	SetActorTickEnabled(true);
	OnPropTransfer.Broadcast(this, Source, nullptr, EStagePropTransferResult::Thrown, CatchWindow);
}

bool AStagePhysicsActor::IsInCatchFlight() const
{
	const UStaticMeshComponent* Body = GetStaticMeshComponent();
	const UWorld* World = GetWorld();
	return World && Thrower.IsValid() && World->GetTimeSeconds() < CatchDeadline && Body->IsSimulatingPhysics()
	    && Body->GetPhysicsLinearVelocity().SizeSquared() > FMath::Square(50.0);
}

void AStagePhysicsActor::StopWatchingThrow()
{
	Thrower.Reset();
	SetActorTickEnabled(false);
}

void AStagePhysicsActor::IgnoreThrowerDuringRelease(const AActor& Source)
{
	RestoreThrowerCollision();
	const auto* Character = Cast<ACharacter>(&Source);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const FBodyInstance* PropBody = GetStaticMeshComponent()->GetBodyInstance();
	const FPhysicsActorHandle Prop = PropBody ? PropBody->GetPhysicsActor() : nullptr;
	if (!Mesh || !Prop) return;

	TArray<FPhysicsActorHandle> ThrowerBodies;
	for (const FBodyInstance* Body : Mesh->Bodies)
		if (Body)
			if (const FPhysicsActorHandle Handle = Body->GetPhysicsActor()) ThrowerBodies.Add(Handle);
	if (ThrowerBodies.IsEmpty()) return;

	const bool bApplied = FPhysicsCommand::ExecuteWrite(Prop, [&](const FPhysicsActorHandle& Handle)
	{
		FChaosEngineInterface::AddDisabledCollisionsFor_AssumesLocked({{Handle, MoveTemp(ThrowerBodies)}});
	});
	// Give the prop time to leave the hand while keeping every other collision active.
	if (bApplied) GetWorld()->GetTimerManager().SetTimer(ThrowerCollisionTimer, this, &ThisClass::RestoreThrowerCollision, 0.12f);
}

void AStagePhysicsActor::RestoreThrowerCollision()
{
	UWorld* World = GetWorld();
	if (!World || !ThrowerCollisionTimer.IsValid()) return;
	World->GetTimerManager().ClearTimer(ThrowerCollisionTimer);
	if (const FBodyInstance* Body = GetStaticMeshComponent()->GetBodyInstance())
		if (const FPhysicsActorHandle Prop = Body->GetPhysicsActor())
			FPhysicsCommand::ExecuteWrite(Prop, [](const FPhysicsActorHandle& Handle)
			{
				TArray<FPhysicsActorHandle> Bodies{Handle};
				FChaosEngineInterface::RemoveDisabledCollisionsFor_AssumesLocked(Bodies);
			});
}

void AStagePhysicsActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World) return;
	auto* Body = GetStaticMeshComponent();
	AActor* Source = Thrower.Get();
	if (!Source || !Body->IsSimulatingPhysics() || World->GetTimeSeconds() >= CatchDeadline)
	{
		StopWatchingThrow();
		if (Source && Body->IsSimulatingPhysics())
		{
			bMissedThrow = true;
			OnPropTransfer.Broadcast(this, Source, nullptr, EStagePropTransferResult::Missed, CatchWindow);
		}
		return;
	}

	const FVector Current = Body->GetCenterOfMass();
	const FVector Previous = PreviousFlightPosition;
	PreviousFlightPosition = Current;
	if (!IsInCatchFlight()) return;
	// Include the upcoming physics step so a fast prop can be secured before contact.
	const FVector Next = Current + Body->GetPhysicsLinearVelocity() * DeltaSeconds
	    + FVector(0.0, 0.0, World->GetGravityZ() * 0.5 * FMath::Square(DeltaSeconds));
	const FVector Midpoint = (Previous + Next) * 0.5;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StagePropCatch), false, this);
	Query.AddIgnoredActor(Source);
	World->OverlapMultiByObjectType(Overlaps, Midpoint, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
	    FCollisionShape::MakeSphere(CatchRadius + FVector::Distance(Previous, Next) * 0.5 + 75.f), Query);

	UPhysicsManipulationComponent* BestReceiver = nullptr;
	double BestDistance = FMath::Square(CatchRadius);
	UStageMemberComponent* NearestWrongReceiver = nullptr;
	double WrongReceiverDistance = BestDistance;
	const double ReleasedAt = CatchDeadline - CatchWindow;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (Candidate == IgnoredCatchActor.Get()) continue;
		auto* Receiver = IsValid(Candidate) ? Candidate->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
		// A throw released on time may still be in flight after the cue closes.
		if (!Receiver || !Receiver->CanReceiveStagePropAt(ReleasedAt)) continue;
		const FVector Hand = Receiver->GetCarryPoint()->GetComponentLocation();
		const FVector Intercept = FMath::ClosestPointOnSegment(Hand, Previous, Next);
		const double Distance = FVector::DistSquared(Hand, Intercept);
		if (Distance >= BestDistance || !HasClearReach(Current, Intercept, Source, Candidate)
		    || !HasClearReach(Intercept, Hand, Source, Candidate) || !HasClearReach(Current, Hand, Source, Candidate)) continue;
		if (!Receiver->CanReceiveStagePropAt(ReleasedAt, this))
		{
			if (Distance < WrongReceiverDistance)
			{
				NearestWrongReceiver = Candidate->FindComponentByClass<UStageMemberComponent>();
				WrongReceiverDistance = Distance;
			}
			continue;
		}
		BestReceiver = Receiver;
		BestDistance = Distance;
	}
	if (!BestReceiver)
	{
		if (NearestWrongReceiver) NearestWrongReceiver->HandleWrongStageProp(ReleasedAt);
		return;
	}

	ActiveManipulator = BestReceiver;
	RestoreThrowerCollision();
	if (!BestReceiver->BeginCatch(*this, *Source, ReleasedAt))
	{
		ActiveManipulator.Reset();
		return;
	}
	StopWatchingThrow();
}

void AStagePhysicsActor::NotifyPropCarried(AActor* Carrier)
{
	if (!bMissedThrow) return;
	bMissedThrow = false;
	OnPropTransfer.Broadcast(this, nullptr, Carrier, EStagePropTransferResult::Recovered, CatchWindow);
}

EInteractionResult AStagePhysicsActor::OnInteraction_Implementation(const FInteractionContext& InteractionContext)
{
	if (ActiveManipulator.IsValid() && ActiveManipulator->GetManipulatedComponent() != GetStaticMeshComponent()) ActiveManipulator.Reset();
	if (UPhysicsManipulationComponent* Manipulator = ActiveManipulator.Get())
	{
		if (Manipulator->GetOwner() != InteractionContext.Interactor) return EInteractionResult::Rejected;

		if (InteractionContext.InteractionPhase == EInteractionPhase::End)
		{
			ActiveManipulator.Reset();
			if (Manipulator->GetManipulatedComponent() == GetStaticMeshComponent())
			{
				Manipulator->EndManipulation(InteractionContext.HoldDuration, InteractionContext.bCancelled);
			}
			return EInteractionResult::Completed;
		}

		return Manipulator->GetManipulatedComponent() == GetStaticMeshComponent() ? EInteractionResult::Active : EInteractionResult::Rejected;
	}

	if (InteractionContext.InteractionPhase != EInteractionPhase::Start || !CanInteract_Implementation(InteractionContext))
	{
		return EInteractionResult::Rejected;
	}

	UPhysicsManipulationComponent* Manipulator = InteractionContext.Interactor->FindComponentByClass<UPhysicsManipulationComponent>();

	ActiveManipulator = Manipulator;
	RestoreThrowerCollision();
	if (!Manipulator->BeginManipulation(InteractionContext.Hit, InteractionType))
	{
		ActiveManipulator.Reset();
		return EInteractionResult::Rejected;
	}
	if (ActiveManipulator.Get() == Manipulator) StopWatchingThrow();
	return InteractionType == EPhysicsInteractionType::Carry ? EInteractionResult::Latched : EInteractionResult::Active;
}

FName AStagePhysicsActor::GetInteractionAnimationId_Implementation() const
{
	static const FName CarryAnimationId(TEXT("Carry"));
	return InteractionType == EPhysicsInteractionType::Carry ? CarryAnimationId : NAME_None;
}

void AStagePhysicsActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreThrowerCollision();
	StopWatchingThrow();
	if (UPhysicsManipulationComponent* Manipulator = ActiveManipulator.Get())
	{
		if (Manipulator->GetManipulatedComponent() == GetStaticMeshComponent()) Manipulator->ReleaseComponent();
	}
	ActiveManipulator.Reset();
	PickupOutline.Reset();
	Super::EndPlay(EndPlayReason);
}
