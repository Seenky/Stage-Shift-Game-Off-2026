// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractionTypes.h"
#include "UObject/Interface.h"
#include "InteractionInterface.generated.h"

UINTERFACE(Blueprintable)
class STAGESHIFT_API UInteractionInterface : public UInterface
{
	GENERATED_BODY()
};


class STAGESHIFT_API IInteractionInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(const FInteractionContext& InteractionContext) const;
	virtual bool CanInteract_Implementation(const FInteractionContext& InteractionContext) const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	EInteractionResult OnInteraction(const FInteractionContext& InteractionContext);
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& InteractionContext);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FName GetInteractionAnimationId() const;
	virtual FName GetInteractionAnimationId_Implementation() const;
};
