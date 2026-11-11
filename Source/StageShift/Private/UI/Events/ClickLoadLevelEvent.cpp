// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Events/ClickLoadLevelEvent.h"

#include "Kismet/GameplayStatics.h"

void UClickLoadLevelEvent::OnClicked_Implementation()
{
	Super::OnClicked_Implementation();
	
	if (LevelToOpen.IsNull()) return;
	
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetWorld(),LevelToOpen);
}
