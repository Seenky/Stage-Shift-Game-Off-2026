#include "Components/Dialogue/DialogueComponent.h"

#include "Components/WidgetComponent.h"
#include "Components/Dialogue/DialogueViewModel.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"

#include "Engine/World.h"
#include "Internationalization/BreakIterator.h"
#include "Misc/Char.h"
#include "TimerManager.h"

UDialogueComponent::UDialogueComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDialogueComponent::BeginPlay()
{
    Super::BeginPlay();

    auto* WidgetComponent = Cast<UWidgetComponent>(DialogueWidgetReference.GetComponent(GetOwner()));
    const UDialogueBubbleWidget* Template = DialogueWidgetTemplate.Get();

    if (!IsValid(WidgetComponent))
    {
        return;
    }

    UDialogueBubbleWidget* DialogueWidget = nullptr;

    if (IsValid(Template))
    {
        DialogueWidget = DuplicateObject<UDialogueBubbleWidget>(Template, WidgetComponent);

        if (ULocalPlayer* LocalPlayer = WidgetComponent->GetOwnerPlayer())
        {
            DialogueWidget->SetOwningLocalPlayer(LocalPlayer);
        }

        DialogueWidget->Initialize();
        WidgetComponent->SetWidget(DialogueWidget);
    }
    else
    {
        WidgetComponent->InitWidget();
        DialogueWidget = Cast<UDialogueBubbleWidget>(WidgetComponent->GetUserWidgetObject());
    }

    if (!IsValid(DialogueWidget))
    {
        return;
    }

    DialogueViewModel = NewObject<UDialogueViewModel>(this);
    DialogueViewModel->Init(this);
    DialogueWidget->SetDialogueViewModel(DialogueViewModel);
}

void UDialogueComponent::SetWidgetComponentProperty(FName ComponentProperty)
{
    DialogueWidgetReference = {};
    DialogueWidgetReference.ComponentProperty = ComponentProperty;
}

void UDialogueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    FinishPlayback(EDialogueEndReason::Interrupted);
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
    RevealedLineData.Reset();
    LastPlaybackState = {};
    LastRevealedLineData.Reset();
    LastRevealDelay = 0.0f;

    Super::EndPlay(EndPlayReason);
}

bool UDialogueComponent::PlayLine(const FDialogueLine& Line, bool bForceStop)
{
    if (bEndingPlay || !HasBegunPlay() || !GetWorld())
    {
        return false;
    }

    if (IsPlaying() && !bForceStop)
    {
        return false;
    }

    const FDialogueLine LineToPlay = Line;

    if (IsPlaying())
    {
        FinishPlayback(EDialogueEndReason::Interrupted);

        if (!IsValid(this) || !HasBegunPlay() || IsPlaying())
        {
            return false;
        }
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return false;
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

    RevealedLineData.LetterEndOffsets.Reset();
    RevealedLineData.RevealedLetterCount = 0;
    RevealedLineData.WordStartOffsets.Reset();
    RevealedLineData.RevealedWordCount = 0;
    bRevealAllRequested = false;

    const TSharedRef<IBreakIterator> Characters = FBreakIterator::CreateCharacterBoundaryIterator();
    Characters->SetStringRef(ResolvedText);
    int32 Start = Characters->ResetToBeginning();
    for (int32 End = Characters->MoveToNext(); End != INDEX_NONE; End = Characters->MoveToNext())
    {
        if (!FChar::IsWhitespace(ResolvedText[Start]))
        {
            RevealedLineData.LetterEndOffsets.Add(End);
            if (Start == 0 || FChar::IsWhitespace(ResolvedText[Start - 1]))
            {
                RevealedLineData.WordStartOffsets.Add(Start);
            }
        }

        Start = End;
    }

    if (RevealedLineData.LetterEndOffsets.IsEmpty())
    {
        const FGuid EmptyPlaybackId = PlaybackState.PlaybackId;
        OnRevealCompleted.Broadcast(EmptyPlaybackId);
        if (IsValid(this) && PlaybackState.PlaybackId == EmptyPlaybackId)
        {
            FinishPlayback(EDialogueEndReason::Completed);
        }
        return true;
    }

    RevealNextLetter();
    
    return true;
}

bool UDialogueComponent::ResumeLine()
{
    if (bEndingPlay || !HasBegunPlay() || !GetWorld())
    {
        return false;
    }

    if (IsPlaying() || !LastRevealedLineData.IsValid() || !LastPlaybackState.IsValid() || LastPlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return false;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    TimerManager.ClearTimer(RevealTimerHandle);
    TimerManager.ClearTimer(HoldTimerHandle);

    PlaybackState = LastPlaybackState;

    PlaybackState.PlaybackId = FGuid::NewGuid();
    ResolvedText = PlaybackState.Line.Text.ToString();
    RevealedLineData = LastRevealedLineData;
    bRevealAllRequested = false;

    const float Delay = LastRevealDelay;
    LastPlaybackState = {};
    LastRevealedLineData.Reset();
    LastRevealDelay = 0.0f;

    const FTimerDelegate ContinuePlayback = FTimerDelegate::CreateUObject(this, &ThisClass::RevealNextLetter);

    if (Delay > 0.0f)
    {
        TimerManager.SetTimer(RevealTimerHandle, ContinuePlayback, Delay, false);
    }
    else
    {
        RevealTimerHandle = TimerManager.SetTimerForNextTick(ContinuePlayback);
    }

    const FDialoguePlaybackState StateSnapshot = PlaybackState;
    OnDialoguePlaybackStateChanged.Broadcast(StateSnapshot);
    
    return true;
}

bool UDialogueComponent::RevealAllWords()
{
    if (PlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
    {
        return false;
    }

    if (bDispatchingWord && DispatchingPlaybackId == PlaybackState.PlaybackId)
    {
        bRevealAllRequested = true;
        return false;
    }

    bRevealAllRequested = false;
    if (NotifyWordsUpTo(ResolvedText.Len()))
    {
        BeginHold();
    }
    
    return true;
}

bool UDialogueComponent::NotifyWordsUpTo(int32 EndOffset)
{
    const FGuid Id = PlaybackState.PlaybackId;
    while (RevealedLineData.RevealedWordCount < RevealedLineData.WordStartOffsets.Num() && RevealedLineData.WordStartOffsets[RevealedLineData.RevealedWordCount] < EndOffset)
    {
        const int32 WordIndex = RevealedLineData.RevealedWordCount++;
        TArray<FDialogueCue, TInlineAllocator<4>> Cues;
        for (const FDialogueCue& Cue : PlaybackState.Line.Cues)
        {
            if (Cue.WordIndex == WordIndex)
            {
                Cues.Add(Cue);
            }
        }
        
        TGuardValue<bool> DispatchGuard(bDispatchingWord, true);
        TGuardValue<FGuid> PlaybackGuard(DispatchingPlaybackId, Id);
        OnWordRevealed.Broadcast(Id, WordIndex);
        if (!IsValid(this) || PlaybackState.PlaybackId != Id ||
            PlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
        {
            return false;
        }

        for (const FDialogueCue& Cue : Cues)
        {
            OnDialogueCueTriggered.Broadcast(Id, Cue);
            if (!IsValid(this) || PlaybackState.PlaybackId != Id ||
                PlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
            {
                return false;
            }
        }
    }

    return IsValid(this) && PlaybackState.PlaybackId == Id && PlaybackState.Phase == EDialoguePlaybackPhase::Revealing;
}

bool UDialogueComponent::StopPlayback()
{
    if (!IsPlaying())
    {
        return false;
    }
    
    FinishPlayback(EDialogueEndReason::Interrupted);
    
    return true;
}

void UDialogueComponent::RevealNextLetter()
{
    if (PlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
    {
        return;
    }

    while (RevealedLineData.RevealedLetterCount < RevealedLineData.LetterEndOffsets.Num())
    {
        const int32 EndOffset = RevealedLineData.LetterEndOffsets[RevealedLineData.RevealedLetterCount++];

        if (!NotifyWordsUpTo(EndOffset))
        {
            return;
        }

        if (bRevealAllRequested)
        {
            RevealAllWords();
            return;
        }

        if (RevealedLineData.RevealedLetterCount == RevealedLineData.LetterEndOffsets.Num())
        {
            BeginHold();
            return;
        }

        const bool bWordBreak = EndOffset < ResolvedText.Len() && FChar::IsWhitespace(ResolvedText[EndOffset]);

        const float Delay = FMath::Max(0.0f, PlaybackState.Line.LetterInterval) +
                            (bWordBreak ? FMath::Max(0.0f, PlaybackState.Line.WordInterval) : 0.0f);

        if (Delay > 0.0f)
        {
            GetWorld()->GetTimerManager().SetTimer(RevealTimerHandle, this, &ThisClass::RevealNextLetter, Delay, false);

            PlaybackState.VisibleText = FText::FromString(ResolvedText.Left(EndOffset));

            const FDialoguePlaybackState StateSnapshot = PlaybackState;
            OnDialoguePlaybackStateChanged.Broadcast(StateSnapshot);

            return;
        }
    }

    BeginHold();
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

    RevealedLineData.RevealedLetterCount = RevealedLineData.LetterEndOffsets.Num();

    PlaybackState.VisibleText = PlaybackState.Line.Text;
    PlaybackState.Phase = EDialoguePlaybackPhase::Holding;

    const FGuid HoldingPlaybackId = PlaybackState.PlaybackId;
    const float HoldDuration = PlaybackState.Line.HoldDuration;

    const bool bWaitBeforeFinishing = HoldDuration > 0.0f;

    if (bWaitBeforeFinishing)
    {
        const FTimerDelegate TimerDelegate =
            FTimerDelegate::CreateUObject(this, &ThisClass::FinishPlayback, EDialogueEndReason::Completed);

        TimerManager.SetTimer(HoldTimerHandle, TimerDelegate, HoldDuration, false);
    }

    const FDialoguePlaybackState StateSnapshot = PlaybackState;

    OnRevealCompleted.Broadcast(HoldingPlaybackId);
    if (!IsValid(this) || PlaybackState.PlaybackId != HoldingPlaybackId ||
        PlaybackState.Phase != EDialoguePlaybackPhase::Holding)
    {
        return;
    }

    OnDialoguePlaybackStateChanged.Broadcast(StateSnapshot);

    if (!bWaitBeforeFinishing && IsValid(this) && PlaybackState.PlaybackId == HoldingPlaybackId &&
        PlaybackState.Phase == EDialoguePlaybackPhase::Holding)
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

    const bool bCanResume = EndReason == EDialogueEndReason::Interrupted &&
        PlaybackState.Phase == EDialoguePlaybackPhase::Revealing;
    LastRevealDelay = 0.0f;
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        if (bCanResume)
        {
            LastRevealDelay = FMath::Max(TimerManager.GetTimerRemaining(RevealTimerHandle), 0.0f);
        }
        TimerManager.ClearTimer(RevealTimerHandle);
        TimerManager.ClearTimer(HoldTimerHandle);
    }

    if (bCanResume)
    {
        LastPlaybackState = PlaybackState;
        LastRevealedLineData = RevealedLineData;
    }
    else
    {
        LastPlaybackState = {};
        LastRevealedLineData.Reset();
    }

    PlaybackState = FDialoguePlaybackState{};
    PlaybackState.Phase = EDialoguePlaybackPhase::Idle;

    ResolvedText.Reset();
    RevealedLineData.Reset();
    bRevealAllRequested = false;

    OnDialogueLineEnded.Broadcast(EndedPlaybackId, EndReason);
}
