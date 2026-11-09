// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageDialogueTypes.h"
#include "Components/ActorComponent.h"
#include "DialogueComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STAGESHIFT_API UDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDialogueComponent();
	
	virtual void BeginPlay() override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void PlayLine(const FDialogueLine& Line, bool bForceStop = false);
	
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void RevealAllWords();
	
	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void StopPlayback();
	
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	const FDialoguePlaybackState& GetPlaybackState() const { return PlaybackState; }
	
	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool IsPlaying() const { return PlaybackState.Phase != EDialoguePlaybackPhase::Idle; }
	
	UPROPERTY(BlueprintAssignable)
	FOnDialoguePlaybackStateChanged OnDialoguePlaybackStateChanged;
	
	UPROPERTY(BlueprintAssignable)
	FOnDialogueLineEnded OnDialogueLineEnded;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(UseComponentPicker, AllowedClasses = "/Script/Engine.WidgetComponent"))
	FComponentReference DialogueWidgetReference;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta=(AllowedClasses = "/Script/StageShift.DialogueWidget"))
	TSubclassOf<UUserWidget> DialogueWidgetClass;
	
private:
	void RevealNextWord();
	
	void BeginHold();
	
	void FinishPlayback(EDialogueEndReason EndReason);
	
	UPROPERTY(Transient)
	FDialoguePlaybackState PlaybackState = {};
	
	UPROPERTY(Transient)
	class UDialogueViewModel* DialogueViewModel = nullptr;
	
	FString ResolvedText = {};
	
	int32 RevealedWordCount = 0;
	
	TArray<int32> WordEndOffsets;
	
	FTimerHandle RevealTimerHandle;
	FTimerHandle HoldTimerHandle;
};
