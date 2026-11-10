// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Components/Interaction/InteractionComponent.h"

#include "StageShift/Public/Components/Interaction/InteractionInterface.h"
#include "Components/Interaction/ThrowAimComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"


UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	if (bResuming)
	{
		const FInteractionAnimation* Animation = FindAnimation(ActiveInteraction);
		const UAnimInstance* Instance = PlaybackInstance.Get();
		if (Animation && Instance && Instance->Montage_IsPlaying(PlayingMontage.Get()) &&
			Instance->Montage_GetCurrentSection(PlayingMontage.Get()) == Animation->StartSection) return;
	}
	FInteractionPayload Payload;
	if (BeginInteraction(FInteractionContext(), Payload) == EInteractionResult::Rejected) return;

	if (const FInteractionAnimation* Animation = FindAnimation(Payload))
	{
		PlaySection(*Animation, bResuming ? Animation->ResumeSection : Animation->StartSection);
	}
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
			ReleasedHoldDuration = GetWorld()->GetTimeSeconds() - PressStartTime;
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
	return Instance && !ReleaseSectionName.IsNone() && Instance->Montage_IsPlaying(PlayingMontage.Get()) &&
		Instance->Montage_GetCurrentSection(PlayingMontage.Get()) == ReleaseSectionName;
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
	
	if (!IsValid(GetOwner()) || !GetWorld() || bDispatchingInteraction || bEndingPlay)
	{
		return EInteractionResult::Rejected;
	}

	TGuardValue<bool> DispatchGuard(bDispatchingInteraction, true);
	
	if (HasActiveInteraction())
	{
		if (ActiveResult != EInteractionResult::Latched) return EInteractionResult::Rejected;

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
	
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams CollisionParams(SCENE_QUERY_STAT(Interaction), false, GetOwner());
	const FVector TraceStart = GetOwner()->GetActorLocation();
	const FVector Forward = GetOwner()->GetActorForwardVector();
	const FVector TraceEnd = TraceStart + Forward * InteractionLength;
	GetWorld()->OverlapMultiByChannel(Overlaps, TraceEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeCapsule(CapsuleRadius, FMath::Max(CapsuleHalfHeight, CapsuleRadius)), CollisionParams);

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
		if (!IsValid(Component) || !IsValid(Target) || !Target->Implements<UInteractionInterface>() || Visited.Contains(Component)) continue;
		Visited.Add(Component);

		FVector Point;
		const bool bHasGrip = Component->DoesSocketExist(TEXT("Grip"));
		if (bHasGrip) Point = Component->GetSocketLocation(TEXT("Grip"));
		else if (Component->GetClosestPointOnCollision(ReachStart, Point) < 0.0f)
			Point = Component->Bounds.GetBox().GetClosestPointTo(ReachStart);

		const FVector Offset = Point - ReachStart;
		const FVector Direction = Offset.GetSafeNormal();
		FHitResult Obstacle;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Obstacle, ReachStart,
			Point + Direction, TraceChannel, CollisionParams) && Obstacle.GetActor() != Target;
		if (bDebug)
		{
			DrawDebugLine(GetWorld(), ReachStart, Point, bBlocked ? FColor::Red : FColor::Green, false, 5.0f);
			DrawDebugSphere(GetWorld(), Point, 2.0f, 8, bBlocked ? FColor::Red : FColor::Green, false, 5.0f);
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
	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score < B.Score; });
	if (bDebug)
	{
		DrawDebugCapsule(GetWorld(), TraceEnd, FMath::Max(CapsuleHalfHeight, CapsuleRadius), CapsuleRadius, FQuat::Identity, Candidates.IsEmpty() ? FColor::Red : FColor::Green, false, 5.0f);
		UE_LOG(LogTemp, Display, TEXT("Interaction: %d overlaps, %d reachable candidates"), Overlaps.Num(), Candidates.Num());
	}

	for (const FCandidate& Candidate : Candidates)
	{
		const FHitResult& Hit = Candidate.Hit;
		AActor* Target = Hit.GetActor();
		if (IsValid(Target) && Target->Implements<UInteractionInterface>())
		{
			FInteractionContext Context;
			Context.ActionTag = InteractionContext.ActionTag;
			Context.Interactor = GetOwner();
			Context.Hit = Hit;
			if (IInteractionInterface::Execute_CanInteract(Target, Context))
			{
				const double StartTime = GetWorld()->GetTimeSeconds();
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
				if (Result != EInteractionResult::Rejected) return Result;
			}
		}
	
	}
	return EInteractionResult::Rejected;
}

bool UInteractionComponent::EndInteraction()
{
	if (bDispatchingInteraction || ActiveResult != EInteractionResult::Active) return false;
	if (UThrowAimComponent* Aim = GetOwner()->FindComponentByClass<UThrowAimComponent>()) Aim->FreezeAim();
	return FinishInteraction(false);
}

void UInteractionComponent::CancelInteraction()
{
	FinishInteraction(true);
	StopInteractionMontage();
}

bool UInteractionComponent::FinishInteraction(bool bCancelled)
{
	AActor* Target = ActiveInteraction.InteractedActor.Get();
	FInteractionContext Context = ActiveContext;
	Context.InteractionPhase = EInteractionPhase::End;
	Context.HoldDuration = ReleaseMontageId != INDEX_NONE
		? ReleasedHoldDuration
		: (GetWorld() ? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - PressStartTime) : 0.0);
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
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
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

