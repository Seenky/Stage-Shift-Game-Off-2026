// Fill out your copyright notice in the Description page of Project Settings.


#include "StageShift/Public/Managers/Stage/StageManager.h"


AStageManager::AStageManager()
{
	PrimaryActorTick.bCanEverTick = true;
	
	StateTreeComponent = CreateDefaultSubobject<UStateTreeComponent>("StateTreeComponent");
	StateTreeComponent->SetStartLogicAutomatically(false);
}