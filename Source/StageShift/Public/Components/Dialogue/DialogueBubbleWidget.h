// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageDialogueTypes.h"
#include "Blueprint/UserWidget.h"
#include "DialogueBubbleWidget.generated.h"


class UDialogueViewModel;

UCLASS()
class STAGESHIFT_API UDialogueBubbleWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintNativeEvent, Category = "Events")
	void OnDialogueViewModelSet(UDialogueViewModel* ViewModel);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Events")
	void OnDialogueLineEnded(FGuid PlaybackId, EDialogueEndReason Reason);
	
	UFUNCTION(BlueprintPure)
	UDialogueViewModel* GetDialogueViewModel() const { return DialogueViewModel; }
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UDialogueViewModel> DialogueViewModel;
};
