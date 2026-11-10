#include "Components/FacialAnimation/FacialAnimationComponent.h"

#include "Components/Dialogue/DialogueComponent.h"
#include "Components/FacialAnimation/FacialAnimationProfile.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

namespace
{
    float UnitWeight(float Value)
    {
        return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
    }

    float SafeTime(float Value)
    {
        return FMath::IsFinite(Value) ? FMath::Max(Value, 0.0f) : 0.0f;
    }
} // namespace

float UFacialAnimationComponent::FBlend::Sample(double Now) const
{
    const float T = Duration > 0.0f ? FMath::Clamp(static_cast<float>((Now - Start) / Duration), 0.0f, 1.0f) : 1.0f;
    return FMath::Lerp(From, To, T * T * (3.0f - 2.0f * T));
}

bool UFacialAnimationComponent::FBlend::IsActive(double Now) const
{
    return From != To && Now < Start + Duration;
}

void UFacialAnimationComponent::FBlend::Set(float Target, float Time, double Now)
{
    From = Sample(Now);
    To = UnitWeight(Target);
    Start = Now;
    Duration = SafeTime(Time);
}

void UFacialAnimationComponent::FOverride::Set(float Value, float Time, double Now)
{
    Weight.Set(Value, Influence.Sample(Now) == 0.0f ? 0.0f : Time, Now);
    Influence.Set(1.0f, Time, Now);
}

void UFacialAnimationComponent::FOverride::Clear(float Time, double Now)
{
    Influence.Set(0.0f, Time, Now);
}

bool UFacialAnimationComponent::FOverride::IsActive(double Now) const
{
    return Influence.IsActive(Now) || (Influence.Sample(Now) > 0.0f && Weight.IsActive(Now));
}

float UFacialAnimationComponent::FOverride::Apply(float Lower, double Now) const
{
    return FMath::Lerp(Lower, Weight.Sample(Now), Influence.Sample(Now));
}

UFacialAnimationComponent::UFacialAnimationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UFacialAnimationComponent::BeginPlay()
{
    Super::BeginPlay();
    USkeletalMeshComponent* Mesh = FaceMesh.Get();
    if (!Mesh)
    {
        Mesh = Cast<USkeletalMeshComponent>(FaceMeshReference.GetComponent(GetOwner()));
    }

    if (!Mesh)
    {
        const ACharacter* Character = Cast<ACharacter>(GetOwner());
        Mesh = Character ? Character->GetMesh() : GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
    }

    SetFaceMesh(Mesh);
    if (bAutoBindDialogue && !BoundDialogue.IsValid())
    {
        BindDialogueComponent(GetOwner()->FindComponentByClass<UDialogueComponent>());
    }
}

void UFacialAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    UnbindDialogueComponent();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AnimationTimer);
    }

    ClearAppliedMorphs();
    Super::EndPlay(EndPlayReason);
}

double UFacialAnimationComponent::GetAnimationTime() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UFacialAnimationComponent::SetProfile(UFacialAnimationProfile* InProfile)
{
    ClearAppliedMorphs();
    Profile = InProfile;
    RebuildChannels();
}

void UFacialAnimationComponent::SetFaceMesh(USkeletalMeshComponent* InMesh)
{
    ClearAppliedMorphs();
    FaceMesh = InMesh;

    if (Channels.IsEmpty())
    {
        RebuildChannels();
        return;
    }

    const USkeletalMesh* Asset = IsValid(InMesh) ? InMesh->GetSkeletalMeshAsset() : nullptr;
    for (FMorphChannel& Channel : Channels)
    {
        Channel.bAvailable = Asset && Asset->FindMorphTarget(Channel.Name);
    }

    UpdateAnimation();
}

void UFacialAnimationComponent::ClearAppliedMorphs()
{
    USkeletalMeshComponent* Mesh = FaceMesh.Get();
    for (FMorphChannel& Channel : Channels)
    {
        if (Mesh && Channel.bApplied && Channel.LastApplied != 0.0f)
        {
            Mesh->SetMorphTarget(Channel.Name, 0.0f);
        }

        Channel.bApplied = false;
    }
}

void UFacialAnimationComponent::RebuildChannels()
{
    Channels.Reset();
    MorphIndices.Reset();
    ControlIndices.Reset();
    bSpeechPulseActive = false;
    bSpeechAttacking = false;
    SpeechInfluence = {};
    if (!Profile)
    {
        UpdateAnimation();
        return;
    }

    const USkeletalMeshComponent* Mesh = FaceMesh.Get();
    const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    auto AddChannel = [&](FName Name, EFacialRegion Region) -> int32
    {
        if (Name.IsNone())
        {
            return INDEX_NONE;
        }

        if (const int32* Existing = MorphIndices.Find(Name))
        {
            if (Channels[*Existing].Region != Region)
            {
                Channels[*Existing].Region = EFacialRegion::FullFace;
            }

            return *Existing;
        }

        const int32 Index = Channels.AddDefaulted();
        FMorphChannel& Channel = Channels[Index];
        Channel.Name = Name;
        Channel.Region = Region;
        Channel.bAvailable = Asset && Asset->FindMorphTarget(Name);
        MorphIndices.Add(Name, Index);
        return Index;
    };
    for (const auto& Expression : Profile->Expressions)
    {
        for (const FFacialMorphTargetValue& Morph : Expression.Value.Morphs)
        {
            AddChannel(Morph.MorphTarget, Morph.Region);
        }
    }

    for (const auto& Binding : Profile->Controls)
    {
        const int32 Index = AddChannel(Binding.Value.MorphTarget, Binding.Value.Region);
        if (Index == INDEX_NONE)
        {
            continue;
        }

        ControlIndices.Add(Binding.Key, Index);
        if (Binding.Key == EFacialControl::MouthOpen || Binding.Key == EFacialControl::MouthWide ||
            Binding.Key == EFacialControl::MouthRound)
        {
            Channels[Index].bSpeechTarget = Binding.Value.Region == EFacialRegion::Mouth;
        }
    }

    for (FMorphChannel& Channel : Channels)
    {
        Channel.bSpeechTarget &= Channel.Region == EFacialRegion::Mouth;
    }

    BlendExpression(0.0f);
}

EFacialExpression UFacialAnimationComponent::GetEffectiveExpression() const
{
    if (bHasDialogueExpression)
    {
        return DialogueExpression;
    }

    return ExpressionStack.IsEmpty() ? BaseExpression : ExpressionStack.Last();
}

void UFacialAnimationComponent::SetExpression(EFacialExpression Expression, float BlendTime)
{
    const EFacialExpression Previous = GetEffectiveExpression();
    BaseExpression = Expression;
    if (GetEffectiveExpression() != Previous)
    {
        BlendExpression(BlendTime);
    }
}

void UFacialAnimationComponent::ClearExpression(float BlendTime)
{
    SetExpression(EFacialExpression::Neutral, BlendTime);
}

void UFacialAnimationComponent::PushExpression(EFacialExpression Expression, float BlendTime)
{
    ExpressionStack.Add(Expression);
    if (!bHasDialogueExpression)
    {
        BlendExpression(BlendTime);
    }
}

void UFacialAnimationComponent::PopExpression(float BlendTime)
{
    if (ExpressionStack.IsEmpty())
    {
        return;
    }

    ExpressionStack.Pop();
    if (!bHasDialogueExpression)
    {
        BlendExpression(BlendTime);
    }
}

void UFacialAnimationComponent::BlendExpression(float BlendTime)
{
    const double Now = GetAnimationTime();
    for (FMorphChannel& Channel : Channels)
    {
        Channel.Expression.Set(0.0f, BlendTime, Now);
    }

    if (Profile)
    {
        if (const FFacialExpressionPreset* Preset = Profile->Expressions.Find(GetEffectiveExpression()))
        {
            for (const FFacialMorphTargetValue& Morph : Preset->Morphs)
            {
                if (const int32* Index = MorphIndices.Find(Morph.MorphTarget))
                {
                    Channels[*Index].Expression.To = UnitWeight(Morph.Weight);
                }
            }
        }
    }

    UpdateAnimation();
}

void UFacialAnimationComponent::SetControlOverride(EFacialControl Control, float Weight, float BlendTime, bool bDialogue)
{
    const int32* Index = ControlIndices.Find(Control);
    if (!Index)
    {
        return;
    }

    FOverride& Override = bDialogue ? Channels[*Index].DialogueControl : Channels[*Index].Control;
    Override.Set(UnitWeight(Weight), BlendTime, GetAnimationTime());
    UpdateAnimation();
}

void UFacialAnimationComponent::SetControl(EFacialControl Control, float Weight, float BlendTime)
{
    SetControlOverride(Control, Weight, BlendTime, false);
}

void UFacialAnimationComponent::ClearControl(EFacialControl Control, float BlendTime)
{
    if (const int32* Index = ControlIndices.Find(Control))
    {
        Channels[*Index].Control.Clear(BlendTime, GetAnimationTime());
        UpdateAnimation();
    }
}

void UFacialAnimationComponent::ResetControls(float BlendTime)
{
    const double Now = GetAnimationTime();
    for (FMorphChannel& Channel : Channels)
    {
        Channel.Control.Clear(BlendTime, Now);
    }

    UpdateAnimation();
}

void UFacialAnimationComponent::StartSpeaking()
{
    bManualSpeaking = true;
}

void UFacialAnimationComponent::StopSpeaking()
{
    bManualSpeaking = false;
    if (!bDialogueSpeaking)
    {
        ReleaseSpeech();
    }
}

void UFacialAnimationComponent::TriggerSpeechPulse()
{
    if (!Profile || bEndingPlay)
    {
        return;
    }
    bool bHasSpeechTarget = false;
    for (const FMorphChannel& Channel : Channels)
    {
        bHasSpeechTarget |= Channel.bSpeechTarget && Channel.bAvailable;
    }

    if (!bHasSpeechTarget)
    {
        return;
    }

    const double Now = GetAnimationTime();
    AdvanceSpeech(Now);
    const float Attack = FMath::Max(SafeTime(Profile->SpeechAttackTime), 0.001f);

    const float Phase = static_cast<float>(++SpeechPulseIndex % 1024) * 2.39996323f;
    const float Amplitude = UnitWeight(Profile->SpeechIntensity * (1.0f + UnitWeight(Profile->SpeechVariation) * FMath::Sin(Phase)));

    for (FMorphChannel& Channel : Channels)
    {
        Channel.Speech.Set(0.0f, Attack, Now);
    }
    auto SetSpeech = [&](EFacialControl Control, float Weight)
    {
        if (const int32* Index = ControlIndices.Find(Control))
        {
            if (Channels[*Index].bSpeechTarget)
            {
                Channels[*Index].Speech.To = UnitWeight(FMath::Max(Channels[*Index].Speech.To, Weight));
            }
        }
    };

    SetSpeech(EFacialControl::MouthOpen, Amplitude);
    SetSpeech(EFacialControl::MouthWide, Amplitude * UnitWeight(Profile->SpeechWideAmount) * (0.5f + 0.5f * FMath::Cos(Phase)));
    SetSpeech(EFacialControl::MouthRound, Amplitude * UnitWeight(Profile->SpeechRoundAmount) * (0.5f - 0.5f * FMath::Cos(Phase)));

    SpeechInfluence.Set(1.0f, Attack, Now);
    SpeechAttackEnd = Now + Attack;
    bSpeechPulseActive = true;
    bSpeechAttacking = true;
    UpdateAnimation();
}

void UFacialAnimationComponent::ReleaseSpeech()
{
    if (!bSpeechPulseActive)
    {
        return;
    }

    const double Now = GetAnimationTime();
    AdvanceSpeech(Now);
    const float Release = Profile ? FMath::Max(SafeTime(Profile->SpeechReleaseTime), 0.001f) : 0.13f;
    SpeechInfluence.Set(0.0f, Release, Now);
    for (FMorphChannel& Channel : Channels)
    {
        Channel.Speech.Set(0.0f, Release, Now);
    }
    bSpeechAttacking = false;
    UpdateAnimation();
}

void UFacialAnimationComponent::AdvanceSpeech(double Now)
{
    if (bSpeechPulseActive && bSpeechAttacking && Now >= SpeechAttackEnd)
    {
        const float Release = Profile ? FMath::Max(SafeTime(Profile->SpeechReleaseTime), 0.001f) : 0.13f;
        SpeechInfluence.Set(0.0f, Release, SpeechAttackEnd);
        for (FMorphChannel& Channel : Channels)
        {
            Channel.Speech.Set(0.0f, Release, SpeechAttackEnd);
        }
        bSpeechAttacking = false;
    }

    if (bSpeechPulseActive && !bSpeechAttacking && !SpeechInfluence.IsActive(Now))
    {
        bSpeechPulseActive = false;
    }
}

void UFacialAnimationComponent::UpdateAnimation()
{
    UWorld* World = GetWorld();
    if (!World || bEndingPlay)
    {
        return;
    }

    const double Now = GetAnimationTime();
    AdvanceSpeech(Now);
    const float SpeechAlpha = SpeechInfluence.Sample(Now);
    bool bAnimating = bSpeechPulseActive;
    USkeletalMeshComponent* Mesh = FaceMesh.Get();
    for (FMorphChannel& Channel : Channels)
    {
        bAnimating |= Channel.Expression.IsActive(Now) || Channel.Control.IsActive(Now) || Channel.DialogueControl.IsActive(Now);

        float Weight = Channel.Control.Apply(Channel.Expression.Sample(Now), Now);
        Weight = Channel.DialogueControl.Apply(Weight, Now);

        if (Channel.Region == EFacialRegion::Mouth || Channel.bSpeechTarget)
        {
            Weight = FMath::Lerp(Weight, Channel.bSpeechTarget ? Channel.Speech.Sample(Now) : 0.0f, SpeechAlpha);
        }

        Weight = UnitWeight(Weight);
        if (Mesh && Channel.bAvailable && (!Channel.bApplied || Channel.LastApplied != Weight))
        {
            Mesh->SetMorphTarget(Channel.Name, Weight);
            Channel.LastApplied = Weight;
            Channel.bApplied = true;
        }
    }

    FTimerManager& Timers = World->GetTimerManager();
    if (bAnimating && Mesh && Profile && HasBegunPlay())
    {
        if (!Timers.IsTimerActive(AnimationTimer))
        {
            const float Rate = FMath::IsFinite(Profile->UpdateRate) ? FMath::Clamp(Profile->UpdateRate, 1.0f, 120.0f) : 30.0f;
            FTimerManagerTimerParameters Parameters;
            Parameters.bLoop = true;
            Parameters.bMaxOncePerFrame = true;
            Timers.SetTimer(AnimationTimer, FTimerDelegate::CreateUObject(this, &ThisClass::UpdateAnimation), 1.0f / Rate, Parameters);
        }
    }
    else
    {
        Timers.ClearTimer(AnimationTimer);
    }
}

void UFacialAnimationComponent::BindDialogueComponent(UDialogueComponent* Dialogue)
{
    if (BoundDialogue.Get() == Dialogue)
    {
        return;
    }

    UnbindDialogueComponent();
    if (!IsValid(Dialogue) || bEndingPlay)
    {
        return;
    }

    BoundDialogue = Dialogue;
    Dialogue->OnDialoguePlaybackStateChanged.AddUniqueDynamic(this, &ThisClass::HandlePlaybackStateChanged);
    Dialogue->OnWordRevealed.AddUniqueDynamic(this, &ThisClass::HandleWordRevealed);
    Dialogue->OnRevealCompleted.AddUniqueDynamic(this, &ThisClass::HandleRevealCompleted);
    Dialogue->OnDialogueCueTriggered.AddUniqueDynamic(this, &ThisClass::HandleCue);
    Dialogue->OnDialogueLineEnded.AddUniqueDynamic(this, &ThisClass::HandleLineEnded);
    HandlePlaybackStateChanged(Dialogue->GetPlaybackState());
}

void UFacialAnimationComponent::UnbindDialogueComponent()
{
    if (UDialogueComponent* Dialogue = BoundDialogue.Get())
    {
        Dialogue->OnDialoguePlaybackStateChanged.RemoveDynamic(this, &ThisClass::HandlePlaybackStateChanged);
        Dialogue->OnWordRevealed.RemoveDynamic(this, &ThisClass::HandleWordRevealed);
        Dialogue->OnRevealCompleted.RemoveDynamic(this, &ThisClass::HandleRevealCompleted);
        Dialogue->OnDialogueCueTriggered.RemoveDynamic(this, &ThisClass::HandleCue);
        Dialogue->OnDialogueLineEnded.RemoveDynamic(this, &ThisClass::HandleLineEnded);
    }

    BoundDialogue.Reset();
    DialoguePlaybackId.Invalidate();
    ClearDialogueOverrides(Profile ? Profile->DialogueBlendTime : 0.2f);
}

bool UFacialAnimationComponent::TrackDialogue(FGuid PlaybackId)
{
    const UDialogueComponent* Dialogue = BoundDialogue.Get();
    if (!Dialogue || !Dialogue->IsPlaying() || Dialogue->GetPlaybackState().PlaybackId != PlaybackId)
    {
        return false;
    }

    if (DialoguePlaybackId != PlaybackId)
    {
        ClearDialogueOverrides(Profile ? Profile->DialogueBlendTime : 0.2f);
        DialoguePlaybackId = PlaybackId;
    }

    return true;
}

void UFacialAnimationComponent::HandlePlaybackStateChanged(const FDialoguePlaybackState& State)
{
    if (State.Phase == EDialoguePlaybackPhase::Idle || !TrackDialogue(State.PlaybackId))
    {
        return;
    }
    bDialogueSpeaking = State.Phase == EDialoguePlaybackPhase::Revealing;
    if (!bDialogueSpeaking && !bManualSpeaking)
    {
        ReleaseSpeech();
    }
}

void UFacialAnimationComponent::HandleWordRevealed(FGuid PlaybackId, int32 WordIndex)
{
    if (!TrackDialogue(PlaybackId))
    {
        return;
    }
    bDialogueSpeaking = true;
    TriggerSpeechPulse();
}

void UFacialAnimationComponent::HandleRevealCompleted(FGuid PlaybackId)
{
    if (DialoguePlaybackId != PlaybackId)
    {
        return;
    }
    bDialogueSpeaking = false;
    if (!bManualSpeaking)
    {
        ReleaseSpeech();
    }
}

void UFacialAnimationComponent::HandleCue(FGuid PlaybackId, const FDialogueCue& Cue)
{
    if (!TrackDialogue(PlaybackId))
    {
        return;
    }
    switch (Cue.Type)
    {
    case EDialogueCueType::SetExpression:
        bHasDialogueExpression = true;
        DialogueExpression = Cue.Expression;
        BlendExpression(Cue.BlendTime);
        break;
    case EDialogueCueType::ClearExpression:
        bHasDialogueExpression = false;
        BlendExpression(Cue.BlendTime);
        break;
    case EDialogueCueType::SetFacialControl:
        SetControlOverride(Cue.Control, Cue.Value, Cue.BlendTime, true);
        break;
    }
}

void UFacialAnimationComponent::ClearDialogueOverrides(float BlendTime)
{
    const bool bRestoreExpression = bHasDialogueExpression;

    bHasDialogueExpression = false;
    bDialogueSpeaking = false;
    const double Now = GetAnimationTime();
    for (FMorphChannel& Channel : Channels)
    {
        Channel.DialogueControl.Clear(BlendTime, Now);
    }

    if (bRestoreExpression)
    {
        BlendExpression(BlendTime);
    }
    else
    {
        UpdateAnimation();
    }

    if (!bManualSpeaking)
    {
        ReleaseSpeech();
    }
}

void UFacialAnimationComponent::HandleLineEnded(FGuid PlaybackId, EDialogueEndReason Reason)
{
    if (DialoguePlaybackId != PlaybackId)
    {
        return;
    }

    DialoguePlaybackId.Invalidate();
    ClearDialogueOverrides(Profile ? Profile->DialogueBlendTime : 0.2f);
}
