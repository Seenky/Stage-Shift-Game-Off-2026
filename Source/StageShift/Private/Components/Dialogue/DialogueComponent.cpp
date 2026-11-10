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

    if (!IsValid(WidgetComponent) || !IsValid(Template))
    {
        return;
    }

    auto* DialogueWidget = DuplicateObject<UDialogueBubbleWidget>(DialogueWidgetTemplate.Get(), WidgetComponent);

    if (!IsValid(DialogueWidget))
    {
        return;
    }

    if (ULocalPlayer* LocalPlayer = WidgetComponent->GetOwnerPlayer())
    {
        DialogueWidget->SetOwningLocalPlayer(LocalPlayer);
    }

    DialogueWidget->Initialize();
    WidgetComponent->SetWidget(DialogueWidget);

    DialogueViewModel = NewObject<UDialogueViewModel>(this);
    DialogueViewModel->Init(this);
    DialogueWidget->SetDialogueViewModel(DialogueViewModel);
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
    LetterEndOffsets.Reset();
    RevealedLetterCount = 0;

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

    LetterEndOffsets.Reset();
    RevealedLetterCount = 0;

    const TSharedRef<IBreakIterator> Characters = FBreakIterator::CreateCharacterBoundaryIterator();
    Characters->SetStringRef(ResolvedText);
    int32 Start = Characters->ResetToBeginning();
    for (int32 End = Characters->MoveToNext(); End != INDEX_NONE; End = Characters->MoveToNext())
    {
        if (!FChar::IsWhitespace(ResolvedText[Start]))
        {
            LetterEndOffsets.Add(End);
        }
        Start = End;
    }
    
    if (LetterEndOffsets.IsEmpty())
    {
        FinishPlayback(EDialogueEndReason::Completed);
        return;
    }
    
    RevealNextLetter();
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

void UDialogueComponent::RevealNextLetter()
{
    if (PlaybackState.Phase != EDialoguePlaybackPhase::Revealing)
    {
        return;
    }

    while (++RevealedLetterCount < LetterEndOffsets.Num())
    {
        const int32 EndOffset = LetterEndOffsets[RevealedLetterCount - 1];
        const bool bWordBreak = FChar::IsWhitespace(ResolvedText[EndOffset]);
        const float Delay = FMath::Max(0.0f, PlaybackState.Line.LetterInterval)
            + (bWordBreak ? FMath::Max(0.0f, PlaybackState.Line.WordInterval) : 0.0f);
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

    RevealedLetterCount = LetterEndOffsets.Num();

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
    LetterEndOffsets.Reset();
    RevealedLetterCount = 0;
    
    OnDialogueLineEnded.Broadcast(EndedPlaybackId, EndReason);
}
