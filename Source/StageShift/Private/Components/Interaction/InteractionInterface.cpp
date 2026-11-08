// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Components/Interaction/InteractionInterface.h"


bool IInteractionInterface::CanInteract_Implementation(const FInteractionContext& InteractionContext) const
{
	return true;
}

EInteractionResult IInteractionInterface::OnInteraction_Implementation(const FInteractionContext& InteractionContext)
{
	return EInteractionResult::Rejected;
}

FName IInteractionInterface::GetInteractionAnimationId_Implementation() const
{
	return NAME_None;
}
