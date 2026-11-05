// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
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
	PreviousCollision = Component->GetCollisionEnabled();
	bWasSimulatingPhysics = Component->IsSimulatingPhysics(Hit.BoneName);

	if (Type == EPhysicsInteractionType::Carry)
	{
		Component->SetSimulatePhysics(false);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (!Component->AttachToComponent(CarryPoint.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale))
		{
			Component->SetCollisionEnabled(PreviousCollision);
			Component->SetSimulatePhysics(bWasSimulatingPhysics);
			return false;
		}
	}
	else
	{
		GrabComponentAtLocation(Component, Hit.BoneName, Hit.ImpactPoint);
		if (GetGrabbedComponent() != Component) return false;
		DragTarget = Hit.ImpactPoint;
		DragHeightOffset = DragTarget.Z - GetOwner()->GetActorLocation().Z;
		SetComponentTickEnabled(true);
	}

	ActiveType = Type;
	ManipulatedComponent = Component;
	ManipulatedActor = Component->GetOwner();
	ManipulatedActor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnManipulatedActorEndPlay);
	return true;
}

void UPhysicsManipulationComponent::EndManipulation(float HoldDuration, bool bCancelled)
{
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	const bool bCarry = ActiveType == EPhysicsInteractionType::Carry;
	const bool bThrow = bCarry && !bCancelled && HoldDuration > TapDuration;

	ReleaseComponent();
	
	if (IsValid(Component) && bCarry && Component->IsSimulatingPhysics() && IsValid(GetOwner()))
	{
		Component->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Component->SetPhysicsLinearVelocity(FVector::ZeroVector);
		if (bThrow)
		{
			const float Charge = FMath::Clamp((HoldDuration - TapDuration) / FMath::Max(MaxChargeDuration, UE_SMALL_NUMBER), 0.0f, 1.0f);
			const float Speed = FMath::Lerp(MinThrowSpeed, FMath::Max(MinThrowSpeed, MaxThrowSpeed), Charge);
			const FVector Direction = (GetOwner()->GetActorForwardVector() + FVector::UpVector * ThrowUpwardBias).GetSafeNormal();
			Component->SetPhysicsLinearVelocity(GetOwner()->GetVelocity());
			Component->AddImpulse(Direction * Speed, NAME_None, true);
		}
	}
}

void UPhysicsManipulationComponent::ReleaseComponent()
{
	if (AActor* Actor = ManipulatedActor.Get())
	{
		Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::OnManipulatedActorEndPlay);
	}
	
	if (UPrimitiveComponent* Component = ManipulatedComponent.Get())
	{
		if (ActiveType == EPhysicsInteractionType::Carry)
		{
			Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Component->SetCollisionEnabled(PreviousCollision);
			Component->SetSimulatePhysics(bWasSimulatingPhysics);
		}
	}
	ManipulatedComponent.Reset();
	ManipulatedActor.Reset();
	ActiveType = EPhysicsInteractionType::None;
	SetComponentTickEnabled(false);
	Super::ReleaseComponent();
}

void UPhysicsManipulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
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
