// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Physics/PhysicsActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
	PrimaryActorTick.bCanEverTick = false;

	GetStaticMeshComponent()->SetCollisionProfileName(TEXT("PhysicsActor"));
	ConfigureCharacterContact();
	GetStaticMeshComponent()->SetSimulatePhysics(true);
	SetMobility(EComponentMobility::Movable);
}

void AStagePhysicsActor::BeginPlay()
{
	ConfigureCharacterContact();
	Super::BeginPlay();
	if (ImpactSound)
	{
		GetStaticMeshComponent()->SetNotifyRigidBodyCollision(true);
		GetStaticMeshComponent()->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleImpactSound);
	}
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
	    || (ActiveManipulator.IsValid() && ActiveManipulator->GetManipulatedComponent() == GetStaticMeshComponent()))
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
}

bool AStagePhysicsActor::IsBeingDragged() const
{
	const auto* Manipulator = ActiveManipulator.Get();
	return Manipulator && Manipulator->IsDragging() && Manipulator->GetManipulatedComponent() == GetStaticMeshComponent();
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
	ActiveManipulator = Receiver;
	if (!Sender->TransferCarriedPropTo(*Receiver))
	{
		ActiveManipulator = Sender;
		return false;
	}
	if (auto* Member = IsValid(From) ? From->FindComponentByClass<UStageMemberComponent>() : nullptr) Member->ShowPartnerInteractionPose();
	if (auto* Member = IsValid(To) ? To->FindComponentByClass<UStageMemberComponent>() : nullptr) Member->ShowPartnerInteractionPose();
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
	GetWorld()->GetTimerManager().SetTimer(CatchTimer, this, &ThisClass::CheckCatch, 0.04f, true);
	OnPropTransfer.Broadcast(this, Source, nullptr, EStagePropTransferResult::Thrown, CatchWindow);
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

void AStagePhysicsActor::CheckCatch()
{
	UWorld* World = GetWorld();
	if (!World) return;
	auto* Body = GetStaticMeshComponent();
	AActor* Source = Thrower.Get();
	if (!Source || !Body->IsSimulatingPhysics() || World->GetTimeSeconds() >= CatchDeadline)
	{
		World->GetTimerManager().ClearTimer(CatchTimer);
		Thrower.Reset();
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
	if (FVector::DistSquared(Current, Previous) < 4.0) return;
	const FVector Midpoint = (Previous + Current) * 0.5;
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StagePropCatch), false, this);
	Query.AddIgnoredActor(Source);
	World->OverlapMultiByObjectType(Overlaps, Midpoint, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
	    FCollisionShape::MakeSphere(CatchRadius + FVector::Distance(Previous, Current) * 0.5 + 75.f), Query);

	UPhysicsManipulationComponent* BestReceiver = nullptr;
	double BestDistance = FMath::Square(CatchRadius);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (Candidate == IgnoredCatchActor.Get()) continue;
		auto* Receiver = IsValid(Candidate) ? Candidate->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
		// A throw released on time may still be in flight after the cue closes.
		if (!Receiver || !Receiver->CanReceiveStagePropAt(CatchDeadline - CatchWindow)) continue;
		const FVector Hand = Receiver->GetCarryPoint()->GetComponentLocation();
		const FVector Intercept = FMath::ClosestPointOnSegment(Hand, Previous, Current);
		const double Distance = FVector::DistSquared(Hand, Intercept);
		if (Distance >= BestDistance || !HasClearReach(Intercept, Hand, Source, Candidate) || !HasClearReach(Current, Hand, Source, Candidate)) continue;
		BestReceiver = Receiver;
		BestDistance = Distance;
	}
	if (!BestReceiver) return;

	AActor* ReceiverActor = BestReceiver->GetOwner();
	ActiveManipulator = BestReceiver;
	RestoreThrowerCollision();
	const FHitResult Hit(this, Body, Body->GetComponentLocation(), FVector::UpVector);
	if (!BestReceiver->BeginManipulationInternal(Hit, EPhysicsInteractionType::Carry, false))
	{
		ActiveManipulator.Reset();
		return;
	}
	// A pickup listener can immediately release the prop and start a new throw.
	if (Thrower.Get() == Source)
	{
		World->GetTimerManager().ClearTimer(CatchTimer);
		Thrower.Reset();
	}
	OnPropTransfer.Broadcast(this, Source, ReceiverActor, EStagePropTransferResult::Caught, CatchWindow);
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
	if (ActiveManipulator.Get() == Manipulator)
	{
		if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(CatchTimer);
		Thrower.Reset();
	}
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
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(CatchTimer);
	Thrower.Reset();
	if (UPhysicsManipulationComponent* Manipulator = ActiveManipulator.Get())
	{
		if (Manipulator->GetManipulatedComponent() == GetStaticMeshComponent()) Manipulator->ReleaseComponent();
	}
	ActiveManipulator.Reset();
	Super::EndPlay(EndPlayReason);
}
