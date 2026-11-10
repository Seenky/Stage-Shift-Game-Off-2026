#pragma once

#include "CoreMinimal.h"
#include "Components/FacialAnimation/FacialAnimationTypes.h"
#include "Engine/DataAsset.h"
#include "FacialAnimationProfile.generated.h"

// Reusable mapping; the component has no character-specific morph names.
UCLASS(BlueprintType)
class STAGESHIFT_API UFacialAnimationProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face")
    TMap<EFacialExpression, FFacialExpressionPreset> Expressions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face")
    TMap<EFacialControl, FFacialControlBinding> Controls;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "1", ClampMax = "120"))
    float UpdateRate = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0"))
    float DialogueBlendTime = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speech", meta = (ClampMin = "0.001"))
    float SpeechAttackTime = 0.045f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speech", meta = (ClampMin = "0.001"))
    float SpeechReleaseTime = 0.13f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speech", meta = (ClampMin = "0", ClampMax = "1"))
    float SpeechIntensity = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speech", meta = (ClampMin = "0", ClampMax = "0.5"))
    float SpeechVariation = 0.16f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speech", meta = (ClampMin = "0", ClampMax = "1"))
    float SpeechWideAmount = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Speech", meta = (ClampMin = "0", ClampMax = "1"))
    float SpeechRoundAmount = 0.3f;
};
