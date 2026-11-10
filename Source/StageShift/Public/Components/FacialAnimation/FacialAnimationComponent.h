// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/Dialogue/StageDialogueTypes.h"
#include "FacialAnimationComponent.generated.h"

class UDialogueComponent;
class UFacialAnimationProfile;
class USkeletalMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UFacialAnimationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFacialAnimationComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable, Category = "Face|Expression")
    void SetExpression(EFacialExpression Expression, float BlendTime = 0.2f);

    UFUNCTION(BlueprintCallable, Category = "Face|Expression")
    void ClearExpression(float BlendTime = 0.2f);

    UFUNCTION(BlueprintPure, Category = "Face|Expression")

    EFacialExpression GetExpression() const
    {
        return BaseExpression;
    }

    UFUNCTION(BlueprintPure, Category = "Face|Expression")
    EFacialExpression GetEffectiveExpression() const;

    UFUNCTION(BlueprintCallable, Category = "Face|Expression")
    void PushExpression(EFacialExpression Expression, float BlendTime = 0.2f);

    UFUNCTION(BlueprintCallable, Category = "Face|Expression")
    void PopExpression(float BlendTime = 0.2f);

    UFUNCTION(BlueprintCallable, Category = "Face|Controls")
    void SetControl(EFacialControl Control, float Weight, float BlendTime = 0.1f);

    UFUNCTION(BlueprintCallable, Category = "Face|Controls")
    void ClearControl(EFacialControl Control, float BlendTime = 0.1f);

    UFUNCTION(BlueprintCallable, Category = "Face|Controls")
    void ResetControls(float BlendTime = 0.1f);

    UFUNCTION(BlueprintCallable, Category = "Face|Speech")
    void StartSpeaking();

    UFUNCTION(BlueprintCallable, Category = "Face|Speech")
    void StopSpeaking();

    UFUNCTION(BlueprintCallable, Category = "Face|Speech")
    void TriggerSpeechPulse();

    UFUNCTION(BlueprintPure, Category = "Face|Speech")
    bool IsSpeaking() const
    {
        return bManualSpeaking || bDialogueSpeaking || bSpeechPulseActive;
    }

    UFUNCTION(BlueprintCallable, Category = "Face")
    void SetProfile(UFacialAnimationProfile* InProfile);

    UFUNCTION(BlueprintCallable, Category = "Face")
    void SetFaceMesh(USkeletalMeshComponent* InMesh);

    UFUNCTION(BlueprintCallable, Category = "Face|Dialogue")
    void BindDialogueComponent(UDialogueComponent* Dialogue);

    UFUNCTION(BlueprintCallable, Category = "Face|Dialogue")
    void UnbindDialogueComponent();

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face")
    TObjectPtr<UFacialAnimationProfile> Profile;

    UPROPERTY(EditAnywhere, Category = "Face",
              meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SkeletalMeshComponent"))
    FComponentReference FaceMeshReference;

    UPROPERTY(EditAnywhere, Category = "Face|Dialogue")
    bool bAutoBindDialogue = true;

private:
    struct FBlend
    {
        float From = 0.0f;
        float To = 0.0f;
        double Start = 0.0;
        float Duration = 0.0f;
        float Sample(double Now) const;
        bool IsActive(double Now) const;
        void Set(float Target, float Time, double Now);
    };

    struct FOverride
    {
        FBlend Weight;
        FBlend Influence;
        void Set(float Value, float Time, double Now);
        void Clear(float Time, double Now);
        bool IsActive(double Now) const;
        float Apply(float Lower, double Now) const;
    };

    struct FMorphChannel
    {
        FName Name;
        EFacialRegion Region = EFacialRegion::FullFace;
        FBlend Expression;
        FOverride Control;
        FOverride DialogueControl;
        FBlend Speech;
        float LastApplied = 0.0f;
        bool bApplied = false;
        bool bAvailable = false;
        bool bSpeechTarget = false;
    };

    void RebuildChannels();
    void ClearAppliedMorphs();
    void BlendExpression(float BlendTime);
    void UpdateAnimation();
    void AdvanceSpeech(double Now);
    void ReleaseSpeech();
    void SetControlOverride(EFacialControl Control, float Weight, float BlendTime, bool bDialogue);
    void ClearDialogueOverrides(float BlendTime);
    bool TrackDialogue(FGuid PlaybackId);
    double GetAnimationTime() const;

    UFUNCTION()
    void HandlePlaybackStateChanged(const FDialoguePlaybackState& State);

    UFUNCTION()
    void HandleWordRevealed(FGuid PlaybackId, int32 WordIndex);

    UFUNCTION()
    void HandleRevealCompleted(FGuid PlaybackId);

    UFUNCTION()
    void HandleCue(FGuid PlaybackId, const FDialogueCue& Cue);

    UFUNCTION()
    void HandleLineEnded(FGuid PlaybackId, EDialogueEndReason Reason);

    UPROPERTY(Transient)
    TWeakObjectPtr<USkeletalMeshComponent> FaceMesh;
    UPROPERTY(Transient)
    TWeakObjectPtr<UDialogueComponent> BoundDialogue;

    TArray<FMorphChannel> Channels;
    TMap<FName, int32> MorphIndices;
    TMap<EFacialControl, int32> ControlIndices;
    TArray<EFacialExpression> ExpressionStack;
    EFacialExpression BaseExpression = EFacialExpression::Neutral;
    EFacialExpression DialogueExpression = EFacialExpression::Neutral;
    bool bHasDialogueExpression = false;
    FGuid DialoguePlaybackId;
    FBlend SpeechInfluence;
    double SpeechAttackEnd = 0.0;
    uint32 SpeechPulseIndex = 0;
    bool bSpeechPulseActive = false;
    bool bSpeechAttacking = false;
    bool bManualSpeaking = false;
    bool bDialogueSpeaking = false;
    bool bEndingPlay = false;
    FTimerHandle AnimationTimer;
};
