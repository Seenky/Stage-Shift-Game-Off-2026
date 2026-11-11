// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Events/ClickExitGameEvent.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

void UClickExitGameEvent::OnClicked_Implementation()
{
	Super::OnClicked_Implementation();
	
	APlayerController* Controller = UGameplayStatics::GetPlayerController(GetWorld(),0);
	if (!Controller) return;
	
	UKismetSystemLibrary::QuitGame(GetWorld(), Controller, EQuitPreference::Quit, true);
}
