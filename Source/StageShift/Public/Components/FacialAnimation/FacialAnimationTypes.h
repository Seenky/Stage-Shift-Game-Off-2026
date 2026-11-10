// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FacialAnimationTypes.generated.h"

UENUM(BlueprintType)
enum class EFacialExpression : uint8
{
    Neutral,
    Happy,
    Sad,
    Angry,
    Surprised,
    Scared,
    Confused
};

UENUM(BlueprintType)
enum class EFacialControl : uint8
{
    // Brows
    BrowRaiseLeft,
    BrowRaiseRight,
    BrowLowerLeft,
    BrowLowerRight,

    // Eyes
    BlinkLeft,
    BlinkRight,
    EyeSquintLeft,
    EyeSquintRight,

    // Mouth
    MouthOpen,
    MouthWide,
    SmileLeft,
    SmileRight,
    MouthRound,
    BrowRotateInLeft,
    BrowRotateInRight,
    BrowRotateOutLeft,
    BrowRotateOutRight,
    MouthRotateClockwise,
    MouthRotateCounterclockwise
};

UENUM(BlueprintType)
enum class EFacialRegion : uint8
{
    Brows,
    Eyes,
    Mouth,
    FullFace
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FFacialMorphTargetValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MorphTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
    float Weight = 1.0f;

    // Metadata about the actual geometry in this morph, not a vertex mask.
    // Mark only mouth-only shapes as Mouth. FullFace shapes remain additive during speech.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFacialRegion Region = EFacialRegion::FullFace;
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FFacialExpressionPreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FFacialMorphTargetValue> Morphs;
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FFacialControlBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MorphTarget;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EFacialRegion Region = EFacialRegion::FullFace;
};
