// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageDialogueTypes.generated.h"

USTRUCT(BlueprintType)
struct FDialogueLine
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Text = {};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float WordInterval = 0.25f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float HoldDuration = 1.0f;
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
	EDialoguePlaybackPhase Phase;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText VisibleText;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid PlaybackId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialoguePlaybackStateChanged, const FDialoguePlaybackState&, PlaybackState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDialogueLineEnded, FGuid, PlaybackId, EDialogueEndReason, Reason);
