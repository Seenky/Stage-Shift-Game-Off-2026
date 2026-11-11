// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/UIClickEventBase.h"

#include "Blueprint/WidgetTree.h"

UWorld* UUIClickEventBase::GetWorld() const
{
	if (AActor* Actor = GetTypedOuter<AActor>())
	{
		return Actor->GetWorld();
	}
	
	if (UWidgetTree* OwningTree = Cast<UWidgetTree>(GetOuter()))
	{
		return OwningTree->GetWorld();
	}
	
	return Super::GetWorld();
}
