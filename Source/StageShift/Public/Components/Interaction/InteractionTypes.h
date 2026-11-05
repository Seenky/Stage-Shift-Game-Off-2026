// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "InteractionTypes.generated.h"

UENUM(BlueprintType)
enum class EInteractionPhase : uint8
{
	Start,
	End,
};

UENUM(BlueprintType)
enum class EInteractionResult : uint8
{
	Rejected,
	Completed,
	Active,
	Latched,
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FInteractionContext
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTag ActionTag;
	
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Interactor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FHitResult Hit;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	EInteractionPhase InteractionPhase = EInteractionPhase::Start;
	
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float HoldDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bCancelled = false;
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FInteractionPayload
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<AActor> InteractedActor;
	
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TWeakObjectPtr<UPrimitiveComponent> InteractedComponent;
	
	bool IsValid() const
	{
		return InteractedActor.IsValid();
	}
	
	void Reset()
	{
		InteractedActor.Reset();
		InteractedComponent.Reset();
	}
};
