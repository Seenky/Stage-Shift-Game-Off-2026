// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/FacialAnimation/FacialAnimationTypes.h"
#include "StageDialogueTypes.generated.h"

UENUM(BlueprintType)
enum class EDialogueCueType : uint8
{
    SetExpression,
    ClearExpression,
    SetFacialControl
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FDialogueCue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    int32 WordIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDialogueCueType Type = EDialogueCueType::SetExpression;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFacialExpression Expression = EFacialExpression::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFacialControl Control = EFacialControl::MouthOpen;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
    float Value = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float BlendTime = 0.2f;
};

USTRUCT(BlueprintType)
struct FDialogueLine
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText SpeakersName = FText::FromString("Aria");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText Text = {};

    // Additional pause when the next letter follows whitespace.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0f, UIMin = 0.0f))
    float WordInterval = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0f, UIMin = 0.0f))
    float HoldDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0f, UIMin = 0.0f))
    float LetterInterval = 0.035f;

    // Zero-based whitespace-delimited words; equal-word cues execute in array order.
    // RevealAllWords dispatches remaining cues. Interrupted lines discard unreached cues.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FDialogueCue> Cues;
};

UENUM(BlueprintType)
enum class EDialoguePlaybackPhase : uint8
{
    Idle,
    Revealing,
    Holding,
};

UENUM(BlueprintType)
enum class EDialogueEndReason : uint8
{
    Completed,
    Interrupted
};

USTRUCT(BlueprintType)
struct FDialoguePlaybackState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDialogueLine Line;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDialoguePlaybackPhase Phase = EDialoguePlaybackPhase::Idle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText VisibleText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGuid PlaybackId;
    
    bool IsValid() const
    {
        return PlaybackId.IsValid() && !Line.Text.IsEmpty();
    }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialoguePlaybackStateChanged, const FDialoguePlaybackState&,
                                            PlaybackState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueLineEnded, FGuid, PlaybackId, EDialogueEndReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueWordRevealed, FGuid, PlaybackId, int32, WordIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueRevealCompleted, FGuid, PlaybackId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueCueTriggered, FGuid, PlaybackId, const FDialogueCue&, Cue);
