// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Components/Interaction/InteractionComponent.h"

#include "StageShift/Public/Components/Interaction/InteractionInterface.h"


UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
}

bool UInteractionComponent::FinishInteraction(bool bCancelled)
{
	AActor* Target = ActiveInteraction.InteractedActor.Get();
	FInteractionContext Context = ActiveContext;
	Context.InteractionPhase = EInteractionPhase::End;
	Context.HoldDuration = GetWorld() ? FMath::Max(0.0, GetWorld()->GetTimeSeconds() - PressStartTime) : 0.0;
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
	ActiveInteraction.Reset();
	ActiveContext = FInteractionContext();
	ActiveResult = EInteractionResult::Rejected;
	PressStartTime = 0.0;
}

void UInteractionComponent::OnTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	ClearActiveInteraction();
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

