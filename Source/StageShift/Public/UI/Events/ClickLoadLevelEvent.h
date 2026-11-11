// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/UIClickEventBase.h"
#include "ClickLoadLevelEvent.generated.h"


UCLASS(DisplayName = "Load Level")
class STAGESHIFT_API UClickLoadLevelEvent : public UUIClickEventBase
{
	GENERATED_BODY()
	
public:
	virtual void OnClicked_Implementation() override;
	
protected:
	UPROPERTY(EditAnywhere, Category = "Settings")
	TSoftObjectPtr<UWorld> LevelToOpen;
};
