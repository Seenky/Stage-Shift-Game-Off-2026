#include "Components/Dialogue/DialogueComponent.h"

#include "Components/WidgetComponent.h"
#include "Components/Dialogue/DialogueViewModel.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"

#include "Engine/World.h"
#include "Misc/Char.h"
#include "TimerManager.h"


UDialogueComponent::UDialogueComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDialogueComponent::BeginPlay()
{
    Super::BeginPlay();

    UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>(DialogueWidgetReference.GetComponent(GetOwner()));

    if (!IsValid(WidgetComponent) || !DialogueWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("DialogueComponent on %s: missing widget component or widget class."), *GetNameSafe(GetOwner()));

        DestroyComponent();
        return;
    }

    WidgetComponent->SetWidgetClass(DialogueWidgetClass);
    WidgetComponent->InitWidget();

    UDialogueBubbleWidget* DialogueWidget = Cast<UDialogueBubbleWidget>(WidgetComponent->GetUserWidgetObject());

    if (!IsValid(DialogueWidget))
    {
        UE_LOG(LogTemp, Warning, TEXT("DialogueComponent on %s: widget must derive from UDialogueBubbleWidget."), *GetNameSafe(GetOwner()));

        DestroyComponent();
        return;
    }

    DialogueViewModel = NewObject<UDialogueViewModel>(this);
    DialogueViewModel->Init(this);

    DialogueWidget->OnDialogueViewModelSet(DialogueViewModel);
}

void UDialogueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RevealTimerHandle);
        World->GetTimerManager().ClearTimer(HoldTimerHandle);
    }

    if (IsValid(DialogueViewModel))
    {
        DialogueViewModel->Deinitialize();
    }

    DialogueViewModel = nullptr;

    PlaybackState = FDialoguePlaybackState{};
    PlaybackState.Phase = EDialoguePlaybackPhase::Idle;

    ResolvedText.Reset();
    WordEndOffsets.Reset();
    RevealedWordCount = 0;

    Super::EndPlay(EndPlayReason);
}

void UDialogueComponent::PlayLine(const FDialogueLine& Line, bool bForceStop)
{
    if (!HasBegunPlay() || !GetWorld())
    {
        return;
    }

    if (IsPlaying() && !bForceStop)
    {
        return;
    }
    
    const FDialogueLine LineToPlay = Line;

    if (IsPlaying())
    {
        FinishPlayback(EDialogueEndReason::Interrupted);
        
        if (!IsValid(this) || !HasBegunPlay() || IsPlaying())
        {
            return;
        }
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    TimerManager.ClearTimer(RevealTimerHandle);
    TimerManager.ClearTimer(HoldTimerHandle);

    PlaybackState = FDialoguePlaybackState{};
    PlaybackState.Line = LineToPlay;
    PlaybackState.PlaybackId = FGuid::NewGuid();
    PlaybackState.Phase = EDialoguePlaybackPhase::Revealing;
    PlaybackState.VisibleText = FText::GetEmpty();

    ResolvedText = LineToPlay.Text.ToString();

    WordEndOffsets.Reset();
    RevealedWordCount = 0;

    const int32 TextLength = ResolvedText.Len();

    for (int32 Index = 0; Index < TextLength; ++Index)
    {
        const bool bIsWordEnd = !FChar::IsWhitespace(ResolvedText[Index]) && (Index + 1 == TextLength || FChar::IsWhitespace(ResolvedText[Index + 1]));

        if (bIsWordEnd)
        {
            WordEndOffsets.Add(Index + 1);
        }
    }
    
    if (WordEndOffsets.IsEmpty())
    {
        FinishPlayback(EDialogueEndReason::Completed);
        return;
    }
    
    if (LineToPlay.WordInterval <= 0.0f)
    {
        RevealAllWords();
        return;
    }
    
    TimerManager.SetTimer(RevealTimerHandle, this, &ThisClass::RevealNextWord, LineToPlay.WordInterval, true);

    RevealNextWord();
}

void UDialogueComponent::RevealAllWords()
{
    if (PlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
    {
        return;
    }

    BeginHold();
}

void UDialogueComponent::StopPlayback()
{
    FinishPlayback(EDialogueEndReason::Interrupted);
}

void UDialogueComponent::RevealNextWord()
{
    if (PlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
    {
        return;
    }

    if (!WordEndOffsets.IsValidIndex(RevealedWordCount))
    {
        FinishPlayback(EDialogueEndReason::Completed);
        return;
    }

    const int32 EndOffset = WordEndOffsets[RevealedWordCount];

    ++RevealedWordCount;
    
    if (RevealedWordCount == WordEndOffsets.Num())
    {
        BeginHold();
        return;
    }

    PlaybackState.VisibleText = FText::FromString(ResolvedText.Left(EndOffset));
    
    const FDialoguePlaybackState StateSnapshot = PlaybackState;

    OnDialoguePlaybackStateChanged.Broadcast(StateSnapshot);
}

void UDialogueComponent::BeginHold()
{
    if (PlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        FinishPlayback(EDialogueEndReason::Interrupted);
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    TimerManager.ClearTimer(RevealTimerHandle);
    TimerManager.ClearTimer(HoldTimerHandle);

    RevealedWordCount = WordEndOffsets.Num();

    PlaybackState.VisibleText = PlaybackState.Line.Text;
    PlaybackState.Phase = EDialoguePlaybackPhase::Holding;

    const FGuid HoldingPlaybackId = PlaybackState.PlaybackId;
    const float HoldDuration = PlaybackState.Line.HoldDuration;

    const bool bWaitBeforeFinishing = HoldDuration > 0.0f;

    if (bWaitBeforeFinishing)
    {
        const FTimerDelegate TimerDelegate = FTimerDelegate::CreateUObject(this, &ThisClass::FinishPlayback, EDialogueEndReason::Completed);

        TimerManager.SetTimer(HoldTimerHandle, TimerDelegate, HoldDuration, false);
    }

    const FDialoguePlaybackState StateSnapshot = PlaybackState;

    OnDialoguePlaybackStateChanged.Broadcast(StateSnapshot);
    
    if (!bWaitBeforeFinishing && IsValid(this) && PlaybackState.PlaybackId == HoldingPlaybackId && PlaybackState.Phase == EDialoguePlaybackPhase::Holding)
    {
        FinishPlayback(EDialogueEndReason::Completed);
    }
}

void UDialogueComponent::FinishPlayback(const EDialogueEndReason EndReason)
{
    if (!IsPlaying())
    {
        return;
    }

    const FGuid EndedPlaybackId = PlaybackState.PlaybackId;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RevealTimerHandle);
        World->GetTimerManager().ClearTimer(HoldTimerHandle);
    }
    
    PlaybackState = FDialoguePlaybackState{};
    PlaybackState.Phase = EDialoguePlaybackPhase::Idle;

    ResolvedText.Reset();
    WordEndOffsets.Reset();
    RevealedWordCount = 0;
    
    OnDialogueLineEnded.Broadcast(EndedPlaybackId, EndReason);
}