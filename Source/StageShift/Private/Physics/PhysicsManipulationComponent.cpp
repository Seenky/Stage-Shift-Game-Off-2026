// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"

UPhysicsManipulationComponent::UPhysicsManipulationComponent()
{
	bAutoActivate = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPhysicsManipulationComponent::SetCarryPoint(USceneComponent* NewCarryPoint)
{
	CarryPoint = NewCarryPoint;
}

bool UPhysicsManipulationComponent::CanManipulate(const FHitResult& Hit, EPhysicsInteractionType Type) const
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!IsValid(GetOwner()) || !IsValid(Component) || ManipulatedComponent.IsValid() || GetGrabbedComponent()
		|| !Component->IsRegistered() || Component->GetWorld() != GetWorld() || Component->GetOwner() == GetOwner()
		|| !Component->IsSimulatingPhysics(Hit.BoneName))
	{
		return false;
	}

	if (Type == EPhysicsInteractionType::Carry)
	{
		return CarryPoint.IsValid() && CarryPoint->GetOwner() == GetOwner() && Component == Component->GetOwner()->GetRootComponent();
	}
	
	return Type == EPhysicsInteractionType::Drag;
}

bool UPhysicsManipulationComponent::BeginManipulation(const FHitResult& Hit, EPhysicsInteractionType Type)
{
	if (!CanManipulate(Hit, Type)) return false;
	UPrimitiveComponent* Component = Hit.GetComponent();
	ActiveType = Type;
	ManipulatedComponent = Component;
	ManipulatedActor = Component->GetOwner();

	if (Type == EPhysicsInteractionType::Carry)
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		UAnimInstance* Instance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		const UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>();
		const FInteractionAnimation* Animation = Interaction ? Interaction->FindAnimationForTarget(Component->GetOwner()) : nullptr;
		UAnimMontage* Montage = Animation ? Animation->Montage.Get() : nullptr;
		const bool bHasGrabNotify = Montage && Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
		{
			return Event.Notify && Event.Notify->GetNotifyName() == TEXT("GrabObject");
		});
		if (Instance && bHasGrabNotify)
		{
			LocalGripLocation = Component->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
			PickupInstance = Instance;
			PickupMontage = Montage;
			bPickupPending = true;
			Instance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &ThisClass::OnPickupNotify);
			Instance->OnMontageEnded.AddUniqueDynamic(this, &ThisClass::OnPickupMontageEnded);
			SetComponentTickEnabled(true);
		}
		else if (!AttachCarriedObject(false))
		{
			ReleaseComponent();
			return false;
		}
	}
	else
	{
		GrabComponentAtLocation(Component, Hit.BoneName, Hit.ImpactPoint);
		if (GetGrabbedComponent() != Component)
		{
			ReleaseComponent();
			return false;
		}
		DragTarget = Hit.ImpactPoint;
		DragHeightOffset = DragTarget.Z - GetOwner()->GetActorLocation().Z;
		SetComponentTickEnabled(true);
	}

	ManipulatedActor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnManipulatedActorEndPlay);
	return true;
}

bool UPhysicsManipulationComponent::AttachCarriedObject(bool bKeepWorldTransform)
{
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	if (!Component || !CarryPoint.IsValid() || !Component->IsSimulatingPhysics()) return false;

	PreviousCollision = Component->GetCollisionEnabled();
	bWasSimulatingPhysics = true;
	Component->SetSimulatePhysics(false);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FAttachmentTransformRules& Rules = bKeepWorldTransform
		? FAttachmentTransformRules::KeepWorldTransform
		: FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	if (!Component->AttachToComponent(CarryPoint.Get(), Rules))
	{
		Component->SetCollisionEnabled(PreviousCollision);
		Component->SetSimulatePhysics(bWasSimulatingPhysics);
		return false;
	}
	bCarryAttached = true;
	return true;
}

bool UPhysicsManipulationComponent::GetPickupTarget(FVector& WorldLocation, float& Weight, FVector* HandOffset) const
{
	const UPrimitiveComponent* Component = ManipulatedComponent.Get();
	const UAnimInstance* Instance = PickupInstance.Get();
	if (!Component || !Instance || !Instance->Montage_IsActive(PickupMontage.Get())) return false;

	WorldLocation = bCarryAttached ? GrabLocation : Component->DoesSocketExist(GripSocket)
		? Component->GetSocketLocation(GripSocket)
		: Component->GetComponentTransform().TransformPosition(LocalGripLocation);
	Weight = FMath::Clamp(Instance->GetCurveValue(TEXT("PickupReach")), 0.0f, 1.0f);
	if (HandOffset && CarryPoint.IsValid())
	{
		const USkeletalMeshComponent* Mesh = Instance->GetSkelMeshComponent();
		const int32 HandIndex = Mesh ? Mesh->GetBoneIndex(TEXT("hand_r")) : INDEX_NONE;
		*HandOffset = HandIndex != INDEX_NONE
			? Mesh->GetBoneTransform(HandIndex).InverseTransformPosition(CarryPoint->GetComponentLocation()) : FVector::ZeroVector;
	}
	return true;
}

void UPhysicsManipulationComponent::OnPickupNotify(FName Name, const FBranchingPointNotifyPayload& Payload)
{
	UAnimInstance* Instance = PickupInstance.Get();
	const FAnimMontageInstance* Montage = Instance ? Instance->GetActiveInstanceForMontage(PickupMontage.Get()) : nullptr;
	if (!bPickupPending || Name != TEXT("GrabObject") || !Montage || Montage->GetInstanceID() != Payload.MontageInstanceID) return;

	float Weight = 0.0f;
	GetPickupTarget(GrabLocation, Weight);
	// An unreachable or moving-away prop stays where it is instead of snapping to the hand.
	if (!CarryPoint.IsValid() || FVector::Dist(CarryPoint->GetComponentLocation(), GrabLocation) > 12.0f ||
		!AttachCarriedObject(true))
	{
		if (UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
		ReleaseComponent();
		return;
	}
	bPickupPending = false;
	SetComponentTickEnabled(false);
}

void UPhysicsManipulationComponent::ClearPickupPlayback()
{
	if (UAnimInstance* Instance = PickupInstance.Get())
	{
		Instance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &ThisClass::OnPickupNotify);
		Instance->OnMontageEnded.RemoveDynamic(this, &ThisClass::OnPickupMontageEnded);
	}
	PickupInstance.Reset();
	PickupMontage.Reset();
}

void UPhysicsManipulationComponent::OnPickupMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* Instance = PickupInstance.Get();
	if (Montage != PickupMontage.Get() || (Instance && Instance->Montage_IsActive(Montage))) return;
	if (bPickupPending)
	{
		if (UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
		ReleaseComponent();
	}
	ClearPickupPlayback();
}

void UPhysicsManipulationComponent::EndManipulation(float HoldDuration, bool bCancelled)
{
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	const bool bCarry = bCarryAttached;
	const bool bThrow = bCarry && !bCancelled && HoldDuration > TapDuration;
	const FVector Forward = !ThrowDirection.IsNearlyZero() ? ThrowDirection : GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;

	ReleaseComponent();
	
	if (IsValid(Component) && bCarry && Component->IsSimulatingPhysics() && IsValid(GetOwner()))
	{
		Component->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Component->SetPhysicsLinearVelocity(FVector::ZeroVector);
		if (bThrow)
		{
			const float Charge = FMath::Clamp((HoldDuration - TapDuration) / FMath::Max(MaxChargeDuration, UE_SMALL_NUMBER), 0.0f, 1.0f);
			const float Speed = FMath::Lerp(MinThrowSpeed, FMath::Max(MinThrowSpeed, MaxThrowSpeed), Charge);
			const FVector Direction = (Forward + FVector::UpVector * ThrowUpwardBias).GetSafeNormal();
			Component->SetPhysicsLinearVelocity(GetOwner()->GetVelocity());
			Component->AddImpulse(Direction * Speed, NAME_None, true);
		}
	}
}

void UPhysicsManipulationComponent::ReleaseComponent()
{
	ClearPickupPlayback();
	if (AActor* Actor = ManipulatedActor.Get())
	{
		Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::OnManipulatedActorEndPlay);
	}
	
	if (UPrimitiveComponent* Component = ManipulatedComponent.Get())
	{
		if (bCarryAttached)
		{
			Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Component->SetCollisionEnabled(PreviousCollision);
			Component->SetSimulatePhysics(bWasSimulatingPhysics);
		}
	}
	ManipulatedComponent.Reset();
	ManipulatedActor.Reset();
	ActiveType = EPhysicsInteractionType::None;
	bPickupPending = false;
	bCarryAttached = false;
	ThrowDirection = FVector::ZeroVector;
	SetComponentTickEnabled(false);
	Super::ReleaseComponent();
}

void UPhysicsManipulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (bPickupPending)
	{
		UAnimInstance* Instance = PickupInstance.Get();
		if (!ManipulatedComponent.IsValid() || !ManipulatedComponent->IsSimulatingPhysics() ||
			!Instance || !Instance->Montage_IsActive(PickupMontage.Get()))
		{
			if (UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
			ReleaseComponent();
		}
		return;
	}
	if (!ManipulatedComponent.IsValid() || !ManipulatedComponent->IsRegistered() || !ManipulatedComponent->IsSimulatingPhysics(GrabbedBoneName) || !IsValid(GetOwner()))
	{
		ReleaseComponent();
		return;
	}
	
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	const FVector Offset = DragTarget - OwnerLocation;
	if (Offset.SizeSquared2D() > FMath::Square(DragDistance))
	{
		DragTarget = OwnerLocation + Offset.GetSafeNormal2D() * DragDistance;
	}
	
	DragTarget.Z = OwnerLocation.Z + DragHeightOffset;
	SetTargetLocation(DragTarget);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UPhysicsManipulationComponent::OnManipulatedActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	ReleaseComponent();
}

void UPhysicsManipulationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseComponent();
	Super::EndPlay(EndPlayReason);
}
