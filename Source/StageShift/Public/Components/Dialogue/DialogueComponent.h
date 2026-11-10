// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageDialogueTypes.h"
#include "Components/ActorComponent.h"
#include "DialogueComponent.generated.h"

class UDialogueBubbleWidget;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UDialogueComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDialogueComponent();

    void SetWidgetComponentProperty(FName ComponentProperty);

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void PlayLine(const FDialogueLine& Line, bool bForceStop = false);

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void RevealAllWords();

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void StopPlayback();

    UFUNCTION(BlueprintPure, Category = "Dialogue")
    const FDialoguePlaybackState& GetPlaybackState() const
    {
        return PlaybackState;
    }

    UFUNCTION(BlueprintPure, Category = "Dialogue")
    bool IsPlaying() const
    {
        return PlaybackState.Phase != EDialoguePlaybackPhase::Idle;
    }
    

    UPROPERTY(BlueprintAssignable)
    FOnDialoguePlaybackStateChanged OnDialoguePlaybackStateChanged;

    UPROPERTY(BlueprintAssignable)
    FOnDialogueLineEnded OnDialogueLineEnded;

    UPROPERTY(BlueprintAssignable, Category = "Dialogue")
    FOnDialogueWordRevealed OnWordRevealed;

    UPROPERTY(BlueprintAssignable, Category = "Dialogue")
    FOnDialogueRevealCompleted OnRevealCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Dialogue")
    FOnDialogueCueTriggered OnDialogueCueTriggered;

protected:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (UseComponentPicker, AllowedClasses = "/Script/UMG.WidgetComponent"))
    FComponentReference DialogueWidgetReference;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Settings")
    TObjectPtr<UDialogueBubbleWidget> DialogueWidgetTemplate;

private:
    void RevealNextLetter();

    bool NotifyWordsUpTo(int32 EndOffset);

    void BeginHold();

    void FinishPlayback(EDialogueEndReason EndReason);

    UPROPERTY(Transient)
    FDialoguePlaybackState PlaybackState = {};

    UPROPERTY(Transient)
    class UDialogueViewModel* DialogueViewModel = nullptr;

    FString ResolvedText = {};

    int32 RevealedLetterCount = 0;

    TArray<int32> LetterEndOffsets;
    TArray<int32> WordStartOffsets;
    int32 RevealedWordCount = 0;
    bool bDispatchingWord = false;
    FGuid DispatchingPlaybackId;
    bool bRevealAllRequested = false;
    bool bEndingPlay = false;

    FTimerHandle RevealTimerHandle;
    FTimerHandle HoldTimerHandle;
};
