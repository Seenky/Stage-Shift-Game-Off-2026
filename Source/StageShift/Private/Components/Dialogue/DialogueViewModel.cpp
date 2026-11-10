#include "Components/Dialogue/DialogueViewModel.h"
#include "Components/Dialogue/DialogueComponent.h"

void UDialogueViewModel::Init(UDialogueComponent* InDialogueComponent)
{
    if (IsValid(InDialogueComponent) && SourceDialogueComponent.Get() == InDialogueComponent)
    {
        return;
    }

    Deinitialize();

    if (!IsValid(InDialogueComponent))
    {
        return;
    }

    SourceDialogueComponent = InDialogueComponent;

    InDialogueComponent->OnDialoguePlaybackStateChanged.AddDynamic(this, &UDialogueViewModel::HandleDialoguePlaybackStateChanged);
    InDialogueComponent->OnDialogueLineEnded.AddDynamic(this, &UDialogueViewModel::HandleDialogueLineEnded);
}

void UDialogueViewModel::Deinitialize()
{
    if (UDialogueComponent* PreviousComponent = SourceDialogueComponent.Get())
    {
        PreviousComponent->OnDialoguePlaybackStateChanged.RemoveDynamic(this, &UDialogueViewModel::HandleDialoguePlaybackStateChanged);

        PreviousComponent->OnDialogueLineEnded.RemoveDynamic(this, &UDialogueViewModel::HandleDialogueLineEnded);
    }

    SourceDialogueComponent.Reset();
}

void UDialogueViewModel::BeginDestroy()
{
    Deinitialize();
    Super::BeginDestroy();
}

void UDialogueViewModel::SetVisibleText(const FText& InText)
{
    UE_MVVM_SET_PROPERTY_VALUE(VisibleText, InText);
}

void UDialogueViewModel::SetPlaybackId(const FGuid& InId)
{
    UE_MVVM_SET_PROPERTY_VALUE(PlaybackId, InId);
}

void UDialogueViewModel::HandleDialoguePlaybackStateChanged(const FDialoguePlaybackState& PlaybackState)
{
    OnPlaybackStateChanged.Broadcast(PlaybackState);
    SetVisibleText(PlaybackState.VisibleText);
    SetPlaybackId(PlaybackState.PlaybackId);
}

void UDialogueViewModel::HandleDialogueLineEnded(FGuid EndedPlaybackId, EDialogueEndReason Reason)
{
    OnDialogueLineEnded.Broadcast(EndedPlaybackId, Reason);
}
