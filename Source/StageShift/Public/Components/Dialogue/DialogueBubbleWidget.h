// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageDialogueTypes.h"
#include "Blueprint/UserWidget.h"
#include "DialogueBubbleWidget.generated.h"


class UDialogueViewModel;
class UDialogueTextWidget;
class UBorder;

UCLASS()
class STAGESHIFT_API UDialogueBubbleWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetDialogueViewModel(UDialogueViewModel* ViewModel);

	UFUNCTION(BlueprintNativeEvent, Category = "Events")
	void OnDialogueViewModelSet(UDialogueViewModel* ViewModel);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UDialogueTextWidget> DialogueText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> DialogueBorder;

private:
	void UpdateBubble(const FDialoguePlaybackState& State);
	void SetBorderOpacity(float Opacity);

	UFUNCTION()
	void EndBubble(FGuid PlaybackId, EDialogueEndReason Reason);

	TWeakObjectPtr<UDialogueViewModel> BoundViewModel;
	FLinearColor BorderColor = FLinearColor::White;
};
