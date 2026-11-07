// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Components/Interaction/InteractionComponent.h"

#include "StageShift/Public/Components/Interaction/InteractionInterface.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"


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

	const FInteractionAnimation* Animation = FindAnimation(ActiveInteraction);
	if (Animation && PlaySection(*Animation, Animation->ReleaseSection))
	{
		UAnimInstance* Instance = PlaybackInstance.Get();
		if (const FAnimMontageInstance* Montage = Instance ? Instance->GetActiveInstanceForMontage(PlayingMontage.Get()) : nullptr)
		{
			// Charge ends at input release. The notify only decides when the result takes effect.
			ReleasedHoldDuration = GetWorld()->GetTimeSeconds() - PressStartTime;
			ReleaseMontageId = Montage->GetInstanceID();
			Instance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &ThisClass::OnMontageNotify);
			FOnMontageEnded OnEnded;
			OnEnded.BindUObject(this, &ThisClass::OnReleaseMontageEnded, ReleaseMontageId);
			Instance->Montage_SetEndDelegate(OnEnded, PlayingMontage.Get());
			return;
		}
	}

	EndInteraction();
	StopInteractionMontage();
}

const FInteractionAnimation* UInteractionComponent::FindAnimation(const FInteractionPayload& Payload) const
{
	AActor* Target = Payload.InteractedActor.Get();
	if (!Target) return nullptr;

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

void UInteractionComponent::OnMontageNotify(FName Name, const FBranchingPointNotifyPayload& Payload)
{
	static const FName CommitNotify(TEXT("CommitInteraction"));
	if (ReleaseMontageId != INDEX_NONE && Payload.MontageInstanceID == ReleaseMontageId && Name == CommitNotify)
	{
		EndInteraction();
	}
}

void UInteractionComponent::OnReleaseMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId)
{
	if (ReleaseMontageId != InstanceId) return;

	// A missing notify still completes a montage that reached its end.
	if (bInterrupted)
	{
		CancelInteraction();
	}
	else
	{
		EndInteraction();
	}
}

void UInteractionComponent::StopInteractionMontage()
{
	UAnimInstance* Instance = PlaybackInstance.Get();
	UAnimMontage* Montage = PlayingMontage.Get();
	PlaybackInstance.Reset();
	PlayingMontage.Reset();
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
	
	TArray<FHitResult> HitResults;
	FCollisionQueryParams CollisionParams(SCENE_QUERY_STAT(Interaction), false, GetOwner());
	const FVector TraceStart = GetOwner()->GetActorLocation();
	const FVector TraceEnd = TraceStart + GetOwner()->GetActorForwardVector() * InteractionLength;
	
	GetWorld()->SweepMultiByChannel(HitResults, TraceStart, TraceEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeCapsule(CapsuleRadius, FMath::Max(CapsuleHalfHeight, CapsuleRadius)), CollisionParams);
	
	HitResults.Sort([](const FHitResult& A, const FHitResult& B)
	{
		return A.Distance < B.Distance;
	}); 

	for (const FHitResult& Hit : HitResults)
	{
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
				return Result;
			}
		}
	
		if (Hit.bBlockingHit) break;
	}
	return EInteractionResult::Rejected;
}

bool UInteractionComponent::EndInteraction()
{
	if (bDispatchingInteraction || ActiveResult != EInteractionResult::Active) return false;
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
	if (AActor* Target = ActiveInteraction.InteractedActor.Get())
	{
		Target->OnEndPlay.RemoveDynamic(this, &ThisClass::OnTargetEndPlay);
	}
	if (ReleaseMontageId != INDEX_NONE)
	{
		if (UAnimInstance* Instance = PlaybackInstance.Get())
		{
			Instance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &ThisClass::OnMontageNotify);
		}
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

