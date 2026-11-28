// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Physics/PhysicsActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Stage/Commands/PickUpCommand.h"
#include "Stage/Core/StageManager.h"
#include "TimerManager.h"
#include "Stage/Environment/StageCurtainRail.h"
#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogStagePropCatch, Log, All);

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

bool AStagePhysicsActor::HasClearReach(FVector From, FVector To, const AActor* Source, const AActor* Receiver, FHitResult* OutBlocker) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StagePropReach), false, this);
	if (Source) Query.AddIgnoredActor(Source);
	if (Receiver) Query.AddIgnoredActor(Receiver);
	if (OutBlocker) return !World->LineTraceSingleByChannel(*OutBlocker, From, To, ECC_Visibility, Query);
	return !World->LineTraceTestByChannel(From, To, ECC_Visibility, Query);
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
	CatchDiagnostic = {};
	bMissedThrow = false;
	SetActorTickEnabled(true);
	OnPropTransfer.Broadcast(this, Source, nullptr, EStagePropTransferResult::Thrown, 0.f);
}

bool AStagePhysicsActor::IsInCatchFlight() const
{
	UStaticMeshComponent* Body = GetStaticMeshComponent();
	const UWorld* World = GetWorld();
	if (!World || !Thrower.IsValid() || !Body->IsSimulatingPhysics() || !Body->RigidBodyIsAwake()) return false;
	if (FMath::Abs(Body->GetPhysicsLinearVelocity().Z) > 25.0) return true;

	// Near the apex of a bounce the vertical speed can be as low as on the floor.
	const FVector Position = Body->GetComponentLocation();
	FComponentQueryParams Query(SCENE_QUERY_STAT(StagePropSupport), this);
	Query.bIgnoreTouches = true;
	TArray<FHitResult> Hits;
	World->ComponentSweepMulti(Hits, Body, Position + FVector::UpVector, Position - FVector::UpVector * 3.0, Body->GetComponentQuat(), Query);
	return !Hits.ContainsByPredicate([](const FHitResult& Hit) { return Hit.bBlockingHit && Hit.ImpactNormal.Z > 0.6; });
}

void AStagePhysicsActor::StopWatchingThrow()
{
	if (CatchDiagnostic.ClosestDistanceSquared < TNumericLimits<double>::Max())
	{
		const AActor* Receiver = CatchDiagnostic.ClosestReceiver.Get();
		UE_LOG(LogStagePropCatch, Display, TEXT("Prop[%s] Receiver[%s] Closest CarryPoint: %.1f cm (PickUp MaxDistance %.1f cm)"),
		    *GetActorNameOrLabel(), Receiver ? *Receiver->GetActorNameOrLabel() : TEXT("None"),
		    FMath::Sqrt(CatchDiagnostic.ClosestDistanceSquared), CatchDiagnostic.ClosestMaxDistance);
	}
	else if (Thrower.IsValid())
	{
		UE_LOG(LogStagePropCatch, Display, TEXT("Prop[%s] No receiver ready during the flight"), *GetActorNameOrLabel());
	}
	CatchDiagnostic = {};
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
	if (!Source || !Body->IsSimulatingPhysics() || !Body->RigidBodyIsAwake())
	{
		StopWatchingThrow();
		if (Source && Body->IsSimulatingPhysics())
		{
			bMissedThrow = true;
			OnPropTransfer.Broadcast(this, Source, nullptr, EStagePropTransferResult::Missed, 0.f);
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
	UPhysicsManipulationComponent* BestReceiver = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	UStageMemberComponent* NearestWrongReceiver = nullptr;
	double WrongReceiverDistance = BestDistance;
	const double Now = World->GetTimeSeconds();
	for (TActorIterator<AStageManager> Stage(World); Stage; ++Stage)
	{
		for (const auto& BoundMember : Stage->GetBoundMembers())
		{
			auto* Member = BoundMember.Get();
			AActor* Candidate = Member ? Member->GetOwner() : nullptr;
			if (!IsValid(Candidate) || Candidate == Source || Candidate == IgnoredCatchActor.Get()) continue;
			const auto* MatchingPickUp = Member->GetOpenPickUpCommand(Now, this);
			const auto* PickUp = MatchingPickUp ? MatchingPickUp : Member->GetOpenPickUpCommand(Now);
			if (!PickUp) continue;
			auto* Receiver = Candidate->FindComponentByClass<UPhysicsManipulationComponent>();
			if (!Receiver || !Receiver->CanReceiveStagePropAt(Now, nullptr, true)) continue;

			const float MaxDistance = PickUp->GetMaxDistance();
			const FVector Hand = Receiver->GetCarryPoint()->GetComponentLocation();
			const FVector Intercept = FMath::ClosestPointOnSegment(Hand, Previous, Next);
			const double Distance = FVector::DistSquared(Hand, Intercept);
			if (Distance < CatchDiagnostic.ClosestDistanceSquared)
			{
				CatchDiagnostic.ClosestDistanceSquared = Distance;
				CatchDiagnostic.ClosestReceiver = Candidate;
				CatchDiagnostic.ClosestMaxDistance = MaxDistance;
			}
			if (Distance > FMath::Square(static_cast<double>(MaxDistance)) || Distance >= BestDistance) continue;

			const auto HasClearCatchSegment = [&](const FVector& From, const FVector& To, const TCHAR* Segment)
			{
				FHitResult Blocker;
				if (HasClearReach(From, To, Source, Candidate, CatchDiagnostic.bLoggedBlocker ? nullptr : &Blocker)) return true;
				if (Blocker.bBlockingHit)
				{
					const AActor* Obstacle = Blocker.GetActor();
					UE_LOG(LogStagePropCatch, Display,
					    TEXT("Prop[%s] Receiver[%s] CarryPoint: %.1f cm (PickUp MaxDistance %.1f cm) / Trace[%s] blocked by Actor[%s] Component[%s] at %s"),
					    *GetActorNameOrLabel(), *Candidate->GetActorNameOrLabel(), FMath::Sqrt(Distance), MaxDistance, Segment,
					    Obstacle ? *Obstacle->GetActorNameOrLabel() : TEXT("None"), *GetNameSafe(Blocker.GetComponent()), *Blocker.ImpactPoint.ToCompactString());
					CatchDiagnostic.bLoggedBlocker = true;
				}
				return false;
			};
			if (!HasClearCatchSegment(Current, Intercept, TEXT("Flight to Intercept"))
			    || !HasClearCatchSegment(Intercept, Hand, TEXT("Intercept to CarryPoint"))
			    || !HasClearCatchSegment(Current, Hand, TEXT("Flight to CarryPoint"))) continue;
			if (!MatchingPickUp)
			{
				if (Distance < WrongReceiverDistance)
				{
					NearestWrongReceiver = Member;
					WrongReceiverDistance = Distance;
				}
				continue;
			}
			BestReceiver = Receiver;
			BestDistance = Distance;
		}
	}
	if (!BestReceiver)
	{
		if (NearestWrongReceiver) NearestWrongReceiver->HandleWrongStageProp(Now);
		return;
	}

	ActiveManipulator = BestReceiver;
	RestoreThrowerCollision();
	if (!BestReceiver->BeginCatch(*this, *Source))
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
	OnPropTransfer.Broadcast(this, nullptr, Carrier, EStagePropTransferResult::Recovered, 0.f);
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
