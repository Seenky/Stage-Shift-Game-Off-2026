// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UIClickEventBase.generated.h"


UCLASS(Abstract, DefaultToInstanced, EditInlineNew)
class STAGESHIFT_API UUIClickEventBase : public UObject
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintNativeEvent, Category = "Events")
	void OnClicked();
	virtual void OnClicked_Implementation() {}
	
#if WITH_EDITOR
	virtual bool ImplementsGetWorld() const override { return true; }
#endif
	
	virtual UWorld* GetWorld() const override;
};
