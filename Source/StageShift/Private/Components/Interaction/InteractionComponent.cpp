// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Components/Interaction/InteractionComponent.h"

#include "StageShift/Public/Components/Interaction/InteractionInterface.h"
#include "Components/Interaction/ThrowAimComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Misc/ScopeExit.h"
#include "Physics/PhysicsActor.h"
#include "DrawDebugHelpers.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	bAutoActivate = true;
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(DeltaTime, TickType, Tick);
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
		RefreshInteractionCandidate();
	else
		SetInteractionCandidate(FHitResult());
}

bool UInteractionComponent::CanResumeInteraction() const
{
	if (!HasActiveInteraction() || ActiveResult != EInteractionResult::Latched) return false;
	const FInteractionAnimation* Animation = FindAnimation(ActiveInteraction);
	const UAnimInstance* Instance = PlaybackInstance.Get();
	return !(Animation && Instance && Instance->Montage_IsPlaying(PlayingMontage.Get()) && Instance->Montage_GetCurrentSection(PlayingMontage.Get()) == Animation->StartSection);
}

void UInteractionComponent::RefreshInteractionCandidate(FGameplayTag ActionTag)
{
	if (bDispatchingInteraction) return;
	FHitResult Hit;
	if (IsActive() && !bEndingPlay && IsValid(GetOwner()) && GetWorld() && !IsPlayingReleaseAnimation() && HandMotion != EInteractionHandMotion::RopeGrip)
	{
		if (!HasActiveInteraction()) Hit = ResolveInteractionCandidate(ActionTag);
		else if (ActiveResult == EInteractionResult::Latched)
		{
			Hit = ResolveHandoffCandidate();
			if (!Hit.GetActor() && Cast<AStagePhysicsActor>(ActiveInteraction.InteractedActor.Get()))
				Hit = ResolveInteractionCandidate(ActionTag, true);
		}
	}
	SetInteractionCandidate(Hit);
}

void UInteractionComponent::SetInteractionCandidate(const FHitResult& Hit)
{
	// Compare weak identities as GetActor() is already null during destruction.
	const bool bChanged = CandidateHit.HitObjectHandle != Hit.HitObjectHandle;
	if (bChanged)
	{
		if (AActor* Previous = CandidateHit.GetActor()) Previous->OnEndPlay.RemoveDynamic(this, &ThisClass::OnCandidateEndPlay);
	}
	CandidateHit = Hit;
	if (bChanged)
	{
		if (AActor* Target = Hit.GetActor()) Target->OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnCandidateEndPlay);
		OnInteractionCandidateChanged.Broadcast(Hit.GetActor());
	}
}

void UInteractionComponent::OnCandidateEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	SetInteractionCandidate(FHitResult());
}

void UInteractionComponent::Deactivate()
{
	Super::Deactivate();
	HandMotion = EInteractionHandMotion::None;
	HandAnchor.Reset();
	SetInteractionCandidate(FHitResult());
}

void UInteractionComponent::OnUnregister()
{
	Deactivate();
	Super::OnUnregister();
}

void UInteractionComponent::BindInput(UEnhancedInputComponent& Input, const UInputAction* Action)
{
	if (!Action) return;

	Input.BindAction(Action, ETriggerEvent::Started, this, &ThisClass::OnInputStarted);
	Input.BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::OnInputReleased);
	Input.BindAction(Action, ETriggerEvent::Canceled, this, &ThisClass::CancelInteraction);
}

void UInteractionComponent::OnInputStarted()
{
	const bool bResuming = ActiveResult == EInteractionResult::Latched;
	FInteractionPayload Payload;
	if (BeginInteraction(FInteractionContext(), Payload) == EInteractionResult::Rejected) return;

	if (const FInteractionAnimation* Animation = FindAnimation(Payload))
	{
		PlaySection(*Animation, bResuming ? Animation->ResumeSection : Animation->StartSection);
	}
}

FHitResult UInteractionComponent::ResolveHandoffCandidate() const
{
	const auto* Prop = Cast<AStagePhysicsActor>(ActiveInteraction.InteractedActor.Get());
	const AActor* Owner = GetOwner();
	if (!Prop || !IsValid(Owner) || !GetWorld() || Prop->GetAttachParentActor() != Owner) return {};

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(StagePropHandoff), false, Owner);
	const FVector Start = Owner->GetActorLocation();
	GetWorld()->OverlapMultiByObjectType(Overlaps, Start, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
	    FCollisionShape::MakeSphere(Prop->GetHandoffRange()), Query);
	double BestDistance = FMath::Square(Prop->GetHandoffRange());
	FHitResult Hit;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsValid(Actor)) continue;
		const FVector Offset = Actor->GetActorLocation() - Start;
		if (Offset.SizeSquared() >= BestDistance || FVector::DotProduct(Owner->GetActorForwardVector(), Offset.GetSafeNormal2D()) < -0.1
		    || !Prop->CanHandOff(Owner, Actor)) continue;
		BestDistance = Offset.SizeSquared();
		Hit = FHitResult(Actor, Overlap.GetComponent(), Actor->GetActorLocation(), -Offset.GetSafeNormal());
	}
	return Hit;
}

bool UInteractionComponent::TryHandOff()
{
	if (ActiveResult != EInteractionResult::Latched || bDispatchingInteraction || bEndingPlay || !IsActive()) return false;
	auto* Prop = Cast<AStagePhysicsActor>(ActiveInteraction.InteractedActor.Get());
	AActor* Receiver = ResolveHandoffCandidate().GetActor();
	if (!Prop || !Receiver) return false;
	{
		TGuardValue<bool> DispatchGuard(bDispatchingInteraction, true);
		if (!Prop->TryHandOff(GetOwner(), Receiver)) return false;
		ClearActiveInteraction();
		StopInteractionMontage();
	}
	RefreshInteractionCandidate();
	return true;
}

void UInteractionComponent::OnInputReleased()
{
	if (ActiveResult != EInteractionResult::Active || ReleaseMontageId != INDEX_NONE) return;
	if (UThrowAimComponent* Aim = GetOwner()->FindComponentByClass<UThrowAimComponent>()) Aim->FreezeAim();

	const FInteractionAnimation* Animation = FindAnimation(ActiveInteraction);
	if (Animation && PlaySection(*Animation, Animation->ReleaseSection))
	{
		UAnimInstance* Instance = PlaybackInstance.Get();
		if (const FAnimMontageInstance* Montage = Instance ? Instance->GetActiveInstanceForMontage(PlayingMontage.Get()) : nullptr)
		{
			// Charge stops now; the timer decides when the result takes effect.
			ReleasedHoldDuration = GetInteractionHoldDuration();
			ReleaseMontageId = Montage->GetInstanceID();
			ReleaseSectionName = Animation->ReleaseSection;
			FOnMontageEnded OnEnded;
			OnEnded.BindUObject(this, &ThisClass::OnReleaseMontageEnded, ReleaseMontageId);
			Instance->Montage_SetEndDelegate(OnEnded, PlayingMontage.Get());
			if (Animation->ReleaseDelay > 0.0f)
			{
				GetWorld()->GetTimerManager().SetTimer(ReleaseTimer, this, &ThisClass::CommitRelease, Animation->ReleaseDelay, false);
			}
			else
			{
				CommitRelease();
			}
			return;
		}
	}

	EndInteraction();
	StopInteractionMontage();
}

const FInteractionAnimation* UInteractionComponent::FindAnimation(const FInteractionPayload& Payload) const
{
	return FindAnimationForTarget(Payload.InteractedActor.Get());
}

const FInteractionAnimation* UInteractionComponent::FindAnimationForTarget(AActor* Target) const
{
	if (!IsValid(Target) || !Target->Implements<UInteractionInterface>()) return nullptr;

	return Animations.Find(IInteractionInterface::Execute_GetInteractionAnimationId(Target));
}

FVector UInteractionComponent::GetControlGripLocation(const UStaticMeshComponent& Mesh, FName SocketName)
{
	if (Mesh.DoesSocketExist(SocketName)) return Mesh.GetSocketLocation(SocketName);
	FVector Min, Max;
	Mesh.GetLocalBounds(Min, Max);
	FVector Top = (Min + Max) * 0.5;
	Top.Z = Max.Z;
	return Mesh.GetComponentTransform().TransformPosition(Top);
}

void UInteractionComponent::PlayHandMotion(EInteractionHandMotion Motion, const FVector& GripLocation, const USceneComponent* Anchor, FName Socket)
{
	if (!IsActive() || !GetWorld() || !IsValid(GetOwner())) return;
	HandMotion = Motion;
	HandAnchor = Anchor;
	HandAnchorSocket = Socket;
	HandContact = Anchor ? Anchor->GetSocketTransform(Socket).InverseTransformPosition(GripLocation) : GripLocation;
	HandMotionStartedAt = GetWorld()->GetTimeSeconds();
}

void UInteractionComponent::ReleaseHandGrip(const USceneComponent& Anchor)
{
	if ((HandMotion == EInteractionHandMotion::Hold || HandMotion == EInteractionHandMotion::RopeGrip)
	    && (!HandAnchor.IsValid() || HandAnchor.Get() == &Anchor))
	{
		HandMotion = EInteractionHandMotion::None;
		HandAnchor.Reset();
	}
}

void UInteractionComponent::SetCatchPreview(bool bShow)
{
	if (!bShow) CatchPreviewStartedAt = -1.0;
	else if (CatchPreviewStartedAt < 0.0 && GetWorld()) CatchPreviewStartedAt = GetWorld()->GetTimeSeconds();
}

bool UInteractionComponent::SampleCatchPreview(FInteractionHandPose& OutPose) const
{
	if (CatchPreviewStartedAt < 0.0) return false;
	const AActor* Owner = GetOwner();
	OutPose = {};
	OutPose.Facing = Owner->GetActorForwardVector().GetSafeNormal2D();
	OutPose.Hands[1] = Owner->GetActorLocation() + OutPose.Facing * 24.0 + Owner->GetActorRightVector() * 32.0 + FVector::UpVector * 53.0;
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - CatchPreviewStartedAt);
	OutPose.Weights[1] = FMath::SmoothStep(0.0f, 0.18f, Elapsed);
	return OutPose.IsActive();
}

bool UInteractionComponent::SampleHandMotion(FInteractionHandPose& OutPose) const
{
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !IsValid(Owner) || !IsActive()) return false;
	if (HandMotion == EInteractionHandMotion::None || HandAnchor.IsStale()) return SampleCatchPreview(OutPose);
	const USceneComponent* Anchor = HandAnchor.Get();
	const FVector Contact = Anchor ? Anchor->GetSocketTransform(HandAnchorSocket).TransformPosition(HandContact) : HandContact;
	const float Elapsed = static_cast<float>(World->GetTimeSeconds() - HandMotionStartedAt);
	const FVector ToGrip = (Contact - Owner->GetActorLocation()).GetSafeNormal2D();
	const FVector Facing = ToGrip.IsNearlyZero() ? Owner->GetActorForwardVector().GetSafeNormal2D() : ToGrip;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing);
	const auto FadeAfter = [Elapsed](float Start, float End) { return 1.0f - FMath::SmoothStep(Start, End, Elapsed); };
	const FVector AboveConsole = Contact - Facing * 32.0 + FVector::UpVector * 27.0;
	OutPose = {};
	OutPose.Facing = Facing;
	OutPose.Hands[1] = Contact;
	switch (HandMotion)
	{
	case EInteractionHandMotion::Catch:
		OutPose.Weights[1] = FadeAfter(0.09f, 0.42f);
		OutPose.Weights[0] = 0.9f * FadeAfter(0.10f, 0.37f);
		OutPose.Hands[0] = Contact - Right * 19.0;
		OutPose.Lean = -Facing * (0.13f * FadeAfter(0.05f, 0.28f));
		OutPose.Compression = 0.024f * OutPose.Weights[1];
		break;
	case EInteractionHandMotion::Receive:
	case EInteractionHandMotion::Give:
		OutPose.Weights[1] = FadeAfter(0.09f, 0.36f);
		OutPose.Lean = Facing * (0.045f * OutPose.Weights[1]);
		break;
	case EInteractionHandMotion::Press:
		OutPose.Weights[1] = FMath::SmoothStep(0.0f, 0.08f, Elapsed) * FadeAfter(0.24f, 0.46f);
		OutPose.Hands[1] = FMath::Lerp(AboveConsole, Contact + FVector::UpVector * 6.0, FMath::SmoothStep(0.06f, 0.20f, Elapsed));
		OutPose.Lean = Facing * (0.09f * OutPose.Weights[1]);
		OutPose.Reach = Facing * (22.0f * OutPose.Weights[1]);
		break;
	case EInteractionHandMotion::Pull:
		OutPose.Weights[1] = FMath::SmoothStep(0.0f, 0.10f, Elapsed) * FadeAfter(0.48f, 0.73f);
		OutPose.Hands[1] = FMath::Lerp(AboveConsole, Contact, FMath::SmoothStep(0.09f, 0.24f, Elapsed));
		OutPose.Lean = -Facing * (0.12f * FMath::SmoothStep(0.24f, 0.44f, Elapsed) * FadeAfter(0.48f, 0.73f));
		OutPose.Reach = Facing * (15.0f * OutPose.Weights[1]);
		break;
	case EInteractionHandMotion::Hold:
		OutPose.Weights[0] = OutPose.Weights[1] = FMath::SmoothStep(0.0f, 0.12f, Elapsed);
		OutPose.Hands[1] = FMath::Lerp(AboveConsole, Contact, FMath::SmoothStep(0.10f, 0.32f, Elapsed)) + Right * 8.0;
		OutPose.Hands[0] = OutPose.Hands[1] - Right * 16.0;
		OutPose.Lean = Facing * (0.07f * OutPose.Weights[1]);
		OutPose.Reach = Facing * (15.0f * OutPose.Weights[1]);
		break;
	case EInteractionHandMotion::RopeGrip:
		OutPose.Weights[0] = FMath::SmoothStep(0.0f, 0.17f, Elapsed);
		OutPose.Hands[0] = Contact;
		OutPose.Reach = Facing * (12.0f * OutPose.Weights[0]);
		break;
	default:
		break;
	}
	return OutPose.IsActive() || SampleCatchPreview(OutPose);
}

bool UInteractionComponent::PlaySection(const FInteractionAnimation& Animation, FName Section)
{
	UAnimMontage* Montage = Animation.Montage;
	if (!Montage || Section.IsNone() || Montage->GetSectionIndex(Section) == INDEX_NONE) return false;

	const AActor* Owner = GetOwner();
	if (!Owner) return false;
	const ACharacter* Character = Cast<ACharacter>(Owner);
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : Owner->FindComponentByClass<USkeletalMeshComponent>();
	UAnimInstance* Instance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!Instance) return false;

	if (PlayingMontage != Montage || PlaybackInstance != Instance || !Instance->Montage_IsPlaying(Montage))
	{
		StopInteractionMontage();
		if (Instance->Montage_Play(Montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false) <= 0.0f)
		{
			return false;
		}
	}
	Instance->Montage_JumpToSection(Section, Montage);

	PlaybackInstance = Instance;
	PlayingMontage = Montage;
	return true;
}

bool UInteractionComponent::IsPlayingReleaseAnimation() const
{
	const UAnimInstance* Instance = PlaybackInstance.Get();
	return Instance && !ReleaseSectionName.IsNone() && Instance->Montage_IsPlaying(PlayingMontage.Get())
	    && Instance->Montage_GetCurrentSection(PlayingMontage.Get()) == ReleaseSectionName;
}

bool UInteractionComponent::IsReleaseImminent() const
{
	const UWorld* World = GetWorld();
	if (!World || ReleaseMontageId == INDEX_NONE) return false;
	const float Remaining = World->GetTimerManager().GetTimerRemaining(ReleaseTimer);
	return Remaining >= 0.0f && Remaining <= 2.0f * World->GetDeltaSeconds();
}

void UInteractionComponent::CommitRelease()
{
	if (ReleaseMontageId == INDEX_NONE) return;
	const UAnimInstance* Instance = PlaybackInstance.Get();
	const FAnimMontageInstance* Montage = Instance ? Instance->GetActiveInstanceForMontage(PlayingMontage.Get()) : nullptr;
	if (Montage && Montage->GetInstanceID() == ReleaseMontageId && Instance->Montage_IsPlaying(PlayingMontage.Get()))
	{
		EndInteraction();
	}
	else
	{
		CancelInteraction();
	}
}

void UInteractionComponent::OnReleaseMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId)
{
	if (ReleaseMontageId != InstanceId) return;

	// A stopped or finished montage cannot leave a delayed release pending.
	CancelInteraction();
}

void UInteractionComponent::StopInteractionMontage()
{
	UAnimInstance* Instance = PlaybackInstance.Get();
	UAnimMontage* Montage = PlayingMontage.Get();
	PlaybackInstance.Reset();
	PlayingMontage.Reset();
	ReleaseSectionName = NAME_None;
	if (Instance && Montage)
	{
		Instance->Montage_Stop(0.08f, Montage);
	}
}

EInteractionResult UInteractionComponent::BeginInteraction(const FInteractionContext& InteractionContext, FInteractionPayload& OutPayload)
{
	OutPayload.Reset();

	if (!IsActive() || !IsValid(GetOwner()) || !GetWorld() || bDispatchingInteraction || bEndingPlay || HandMotion == EInteractionHandMotion::RopeGrip)
	{
		return EInteractionResult::Rejected;
	}
	if (TryHandOff()) return EInteractionResult::Completed;

	RefreshInteractionCandidate(InteractionContext.ActionTag);
	ON_SCOPE_EXIT
	{
		RefreshInteractionCandidate();
	};
	TGuardValue<bool> DispatchGuard(bDispatchingInteraction, true);

	if (HasActiveInteraction())
	{
		// A one-shot control can use the free hand while the carried prop keeps its latched interaction.
		AActor* Candidate = CandidateHit.GetActor();
		const auto* Control = Cast<IInteractionInterface>(Candidate);
		if (Control && Candidate != ActiveInteraction.InteractedActor.Get() && Control->CanInteractWhileCarrying())
		{
			FInteractionContext Context;
			Context.ActionTag = InteractionContext.ActionTag;
			Context.Interactor = GetOwner();
			Context.Hit = CandidateHit;
			const EInteractionResult Result = IInteractionInterface::Execute_OnInteraction(Candidate, Context);
			if (Result == EInteractionResult::Completed)
			{
				OutPayload.InteractedActor = Candidate;
				OutPayload.InteractedComponent = Context.Hit.GetComponent();
			}
			return Result;
		}

		if (!CanResumeInteraction()) return EInteractionResult::Rejected;

		AActor* Target = ActiveInteraction.InteractedActor.Get();
		FInteractionContext Context = ActiveContext;
		Context.InteractionPhase = EInteractionPhase::Start;
		Context.HoldDuration = 0.0f;
		Context.bCancelled = false;
		PressStartTime = GetWorld()->GetTimeSeconds();

		const EInteractionResult Result = IInteractionInterface::Execute_OnInteraction(Target, Context);
		if (Result != EInteractionResult::Rejected && HasActiveInteraction())
		{
			OutPayload = ActiveInteraction;
			ActiveResult = Result;
			if (Result == EInteractionResult::Completed) ClearActiveInteraction();
		}
		return Result;
	}

	ClearActiveInteraction();

	const FHitResult Hit = CandidateHit;
	AActor* Target = Hit.GetActor();
	if (!IsValid(Target)) return EInteractionResult::Rejected;

	FInteractionContext Context;
	Context.ActionTag = InteractionContext.ActionTag;
	Context.Interactor = GetOwner();
	Context.Hit = Hit;
	const double StartTime = GetWorld()->GetTimeSeconds();
	// Never fall through to an unpreviewed target if execution rejects this candidate.
	const EInteractionResult Result = IInteractionInterface::Execute_OnInteraction(Target, Context);
	if (Result != EInteractionResult::Rejected && IsValid(Target) && !Target->IsActorBeingDestroyed() && !bEndingPlay)
	{
		OutPayload.InteractedActor = Target;
		OutPayload.InteractedComponent = Hit.GetComponent();
		if (Result == EInteractionResult::Active || Result == EInteractionResult::Latched)
		{
			ActiveInteraction = OutPayload;
			ActiveContext = Context;
			ActiveResult = Result;
			PressStartTime = StartTime;
			Target->OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnTargetEndPlay);
		}
	}
	return Result;
}

FHitResult UInteractionComponent::ResolveInteractionCandidate(FGameplayTag ActionTag, bool bWhileCarrying) const
{
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams CollisionParams(SCENE_QUERY_STAT(Interaction), false, GetOwner());
	const FVector TraceStart = GetOwner()->GetActorLocation();
	const FVector Forward = GetOwner()->GetActorForwardVector();
	const FVector TraceEnd = TraceStart + Forward * InteractionLength;
	GetWorld()->OverlapMultiByChannel(
	    Overlaps, TraceEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeCapsule(CapsuleRadius, FMath::Max(CapsuleHalfHeight, CapsuleRadius)), CollisionParams);

	FVector ReachStart = TraceStart;
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const USkeletalMeshComponent* Mesh = Character->GetMesh();
		const int32 Shoulder = Mesh ? Mesh->GetBoneIndex(TEXT("upperarm_r")) : INDEX_NONE;
		if (Shoulder != INDEX_NONE) ReachStart = Mesh->GetBoneTransform(Shoulder).GetLocation();
	}

	struct FCandidate
	{
		FHitResult Hit;
		double Score;
	};

	TArray<FCandidate> Candidates;
	TSet<UPrimitiveComponent*> Visited;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Component) || !IsValid(Target) || Target->IsActorBeingDestroyed() || !Target->Implements<UInteractionInterface>() || Visited.Contains(Component)) continue;
		if (bWhileCarrying)
		{
			const auto* Control = Cast<IInteractionInterface>(Target);
			if (!Control || !Control->CanInteractWhileCarrying()) continue;
		}
		Visited.Add(Component);

		FVector Point;
		const bool bHasGrip = Component->DoesSocketExist(TEXT("Grip"));
		if (bHasGrip)
			Point = Component->GetSocketLocation(TEXT("Grip"));
		else if (Component->GetClosestPointOnCollision(ReachStart, Point) < 0.0f)
			Point = Component->Bounds.GetBox().GetClosestPointTo(ReachStart);

		const FVector Offset = Point - ReachStart;
		const FVector Direction = Offset.GetSafeNormal();
		FHitResult Obstacle;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Obstacle, ReachStart, Point + Direction, TraceChannel, CollisionParams) && Obstacle.GetActor() != Target;
		if (bDebug)
		{
			DrawDebugLine(GetWorld(), ReachStart, Point, bBlocked ? FColor::Red : FColor::Green, false, 0.0f);
			DrawDebugSphere(GetWorld(), Point, 2.0f, 8, bBlocked ? FColor::Red : FColor::Green, false, 0.0f);
		}
		if (bBlocked) continue;

		FHitResult Hit(Target, Component, Point, -Direction);
		// Preserve the physical bone when the candidate is a skeletal component.
		FHitResult SurfaceHit;
		if (Component->LineTraceComponent(SurfaceHit, ReachStart, Point + Direction, CollisionParams)) Hit = SurfaceHit;
		Hit.ImpactPoint = Point;
		Hit.Location = Point;
		Hit.TraceStart = ReachStart;
		Hit.TraceEnd = Point;
		Hit.Distance = Offset.Size();
		const double Score = 1.0 - FVector::DotProduct(Forward, Direction) + 0.1 * Hit.Distance / FMath::Max(InteractionLength + CapsuleRadius, 1.0f);
		Candidates.Add({Hit, Score});
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		return A.Score < B.Score;
	});
	if (bDebug)
	{
		DrawDebugCapsule(
		    GetWorld(), TraceEnd, FMath::Max(CapsuleHalfHeight, CapsuleRadius), CapsuleRadius, FQuat::Identity, Candidates.IsEmpty() ? FColor::Red : FColor::Green, false, 0.0f);
	}

	for (const FCandidate& Candidate : Candidates)
	{
		FInteractionContext Context;
		Context.ActionTag = ActionTag;
		Context.Interactor = GetOwner();
		Context.Hit = Candidate.Hit;
		if (IInteractionInterface::Execute_CanInteract(Candidate.Hit.GetActor(), Context)) return Candidate.Hit;
	}
	return FHitResult();
}

bool UInteractionComponent::EndInteraction()
{
	if (bDispatchingInteraction || ActiveResult != EInteractionResult::Active) return false;
	if (UThrowAimComponent* Aim = GetOwner()->FindComponentByClass<UThrowAimComponent>()) Aim->FreezeAim();
	return FinishInteraction(false);
}

float UInteractionComponent::GetInteractionHoldDuration() const
{
	if (ReleaseMontageId != INDEX_NONE) return ReleasedHoldDuration;
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0, World->GetTimeSeconds() - PressStartTime) : 0.0f;
}

void UInteractionComponent::CancelInteraction()
{
	FinishInteraction(true);
	StopInteractionMontage();
}

bool UInteractionComponent::FinishInteraction(bool bCancelled)
{
	if (bDispatchingInteraction) return false;
	ON_SCOPE_EXIT
	{
		RefreshInteractionCandidate();
	};
	AActor* Target = ActiveInteraction.InteractedActor.Get();
	FInteractionContext Context = ActiveContext;
	Context.InteractionPhase = EInteractionPhase::End;
	Context.HoldDuration = GetInteractionHoldDuration();
	Context.bCancelled = bCancelled;
	ClearActiveInteraction();

	if (!IsValid(Target) || Target->IsActorBeingDestroyed()) return false;

	TGuardValue<bool> DispatchGuard(bDispatchingInteraction, true);
	IInteractionInterface::Execute_OnInteraction(Target, Context);
	return true;
}

void UInteractionComponent::ClearActiveInteraction()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ReleaseTimer);
	if (AActor* Target = ActiveInteraction.InteractedActor.Get())
	{
		Target->OnEndPlay.RemoveDynamic(this, &ThisClass::OnTargetEndPlay);
	}
	if (ReleaseMontageId != INDEX_NONE)
	{
		ReleaseMontageId = INDEX_NONE;
		ReleasedHoldDuration = 0.0f;
	}
	ActiveInteraction.Reset();
	ActiveContext = FInteractionContext();
	ActiveResult = EInteractionResult::Rejected;
	PressStartTime = 0.0;
}

void UInteractionComponent::OnTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	ClearActiveInteraction();
	StopInteractionMontage();
	RefreshInteractionCandidate();
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	Deactivate();
	CancelInteraction();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void UInteractionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UInteractionComponent, CapsuleHalfHeight))
	{
		CapsuleHalfHeight = FMath::Max3(0.f, CapsuleHalfHeight, CapsuleRadius);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UInteractionComponent, CapsuleRadius))
	{
		CapsuleRadius = FMath::Clamp(CapsuleRadius, 0.f, CapsuleHalfHeight);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
