// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/UIClickEventBase.h"
#include "ClickExitGameEvent.generated.h"


UCLASS(DisplayName = "Exit Game")
class STAGESHIFT_API UClickExitGameEvent : public UUIClickEventBase
{
	GENERATED_BODY()
	
public:
	virtual void OnClicked_Implementation() override;
};
