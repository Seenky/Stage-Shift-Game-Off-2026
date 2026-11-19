#pragma once

#include "Components/Interaction/InteractionComponent.h"
#include "Components/Interaction/InteractionInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "Physics/PhysicsManipulationComponent.h"

namespace StageInteraction
{
	inline AActor* GetOwner(const FStageExecutionContext& Context)
	{
		const auto* Member = Context.MemberComponent.Get();
		return Member ? Member->GetOwner() : nullptr;
	}

	inline bool IsHolding(const UPhysicsManipulationComponent* Manipulator, const AActor* Item)
	{
		const auto* Component = IsValid(Manipulator) ? Manipulator->GetManipulatedComponent() : nullptr;
		return IsValid(Item) && Manipulator && Manipulator->IsCarrying() && IsValid(Component) && Component->GetOwner() == Item;
	}

	inline const FInteractionAnimation* FindAnimation(AActor* Owner, AActor* Target)
	{
		const auto* Interaction = IsValid(Owner) ? Owner->FindComponentByClass<UInteractionComponent>() : nullptr;
		return Interaction ? Interaction->FindAnimationForTarget(Target) : nullptr;
	}

	inline bool MakeContext(AActor* Owner, AActor* Target, FGameplayTag ActionTag, EInteractionPhase Phase, float MaxDistance, FInteractionContext& Out, FString* OutFailure = nullptr)
	{
		if (OutFailure) OutFailure->Reset();
		const auto Reject = [OutFailure](const FString& Details)
		{
			if (OutFailure) *OutFailure = Details;
			return false;
		};
		if (!IsValid(Owner) || !IsValid(Target)) return Reject(TEXT("Invalid interactor or target"));
		if (Owner == Target) return Reject(TEXT("Interactor cannot interact with itself"));
		if (!Owner->GetWorld() || Target->GetWorld() != Owner->GetWorld()) return Reject(TEXT("Interactor and target must belong to the same world"));
		if (Target->IsActorBeingDestroyed()) return Reject(TEXT("Target is being destroyed"));
		if (!Target->Implements<UInteractionInterface>()) return Reject(TEXT("Target does not implement InteractionInterface"));
		auto* Component = Cast<UPrimitiveComponent>(Target->GetRootComponent());
		if (!Component) Component = Target->FindComponentByClass<UPrimitiveComponent>();
		const FVector Start = Owner->GetActorLocation();
		FVector Point = Target->GetActorLocation();
		if (Component && Component->GetClosestPointOnCollision(Start, Point) < 0.0f) Point = Component->Bounds.GetBox().GetClosestPointTo(Start);
		if (Phase == EInteractionPhase::Start)
		{
			if (!FMath::IsFinite(MaxDistance) || MaxDistance <= 0) return Reject(TEXT("MaxDistance must be positive and finite"));
			if (FVector::DistSquared(Start, Point) > FMath::Square(MaxDistance))
				return Reject(FString::Printf(TEXT("Target out of reach: Distance=%.1f cm, MaxDistance=%.1f cm, From=%s, To=%s"), FVector::Distance(Start, Point), MaxDistance, *Start.ToString(), *Point.ToString()));
			FHitResult Obstacle;
			FCollisionQueryParams Query(SCENE_QUERY_STAT(StageInteraction), false, Owner);
			if (Owner->GetWorld()->LineTraceSingleByChannel(Obstacle, Start, Point, ECC_Visibility, Query) && Obstacle.GetActor() != Target)
				return Reject(FString::Printf(TEXT("Target blocked by Actor[%s], Component[%s], Hit=%s"), *GetNameSafe(Obstacle.GetActor()), *GetNameSafe(Obstacle.GetComponent()), *Obstacle.ImpactPoint.ToString()));
		}
		Out.Interactor = Owner;
		Out.ActionTag = ActionTag;
		Out.InteractionPhase = Phase;
		Out.Hit = FHitResult(Target, Component, Point, (Start - Point).GetSafeNormal());
		Out.Hit.TraceStart = Start;
		Out.Hit.TraceEnd = Point;
		Out.Hit.Distance = FVector::Distance(Start, Point);
		return true;
	}
}
