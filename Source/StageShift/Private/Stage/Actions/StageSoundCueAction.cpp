#include "Stage/Actions/StageSoundCueAction.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Cues/StageCueComponent.h"
#include "StageInteractionHelpers.h"
#include "TimerManager.h"

void UStageSoundCueAction::Setup(
    AStageSoundControl* InControl, UAnimSequenceBase* InWindupAnimation, FName InAnimationSlot, FName InCueAnchorId, bool bInRequireOnTime,
    FName InCueId)
{
	Control = InControl;
	WindupAnimation = InWindupAnimation;
	AnimationSlot = InAnimationSlot;
	CueAnchorId = InCueAnchorId;
	bRequireOnTime = bInRequireOnTime;
	CueId = InCueId;
}

void UStageSoundCueAction::SetupSound(FName InSoundId, AActor* InSoundTarget, const FStageSoundTiming& InTiming)
{
	SoundId = InSoundId;
	SoundTarget = InSoundTarget;
	Timing = InTiming;
}

void UStageSoundCueAction::OnStart_Implementation()
{
	auto* Console = Control.Get();
	UWorld* World = GetWorld();
	if (!Console || !World || !(SoundId.IsNone() ? Console->OpenWindow() : Console->OpenSoundWindow(SoundId, SoundTarget.Get(), Timing)))
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	const FStageSoundWindow Window = Console->GetSoundWindow();
	WindowId = Window.Id;
	if (GetState() != EStageExecutionState::Running)
	{
		Console->CancelWindow();
		return;
	}
	const float Duration = Window.ExpiresAt - Window.OpenedAt;
	WindupDuration = FMath::Max(0.1f, Window.ExpiresAt - Window.EarliestAt);
	Console->OnCueResult.AddUniqueDynamic(this, &ThisClass::HandleSoundResult);
	if (AStageManager* Manager = Context.Manager.Get(); Manager && Manager->PlayerCues)
	{
		FStageCueDefinition Cue;
		Cue.CueId = CueId;
		Cue.Kind = EStageCueKind::Sound;
		if (!CueAnchorId.IsNone())
		{
			FStageCueTarget& Marker = Cue.Targets.AddDefaulted_GetRef();
			Marker.Type = EStageCueMarkerType::Object;
			Marker.BindingId = CueAnchorId;
		}
		else if (const UStageDataAsset* Stage = Manager->GetActiveStage())
			for (const FName RoleId : Stage->GetRoleOptions())
				if (Manager->GetParticipant(RoleId) == StageInteraction::GetOwner(Context))
				{
					Cue.SubjectRoleId = RoleId;
					break;
				}
		Cue.TimeLimit = Duration;
		if (auto* Member = Context.MemberComponent.Get(); Member && Manager->PlayerCues->ShowActionCue(Cue, *Member))
		{
			bCueShown = true;
			Manager->PlayerCues->SetSoundWindow(CueId, Window);
		}
	}

	const float WindupDelay = Window.EarliestAt - Window.OpenedAt;
	if (WindupAnimation && WindupDelay > 0.0f)
		World->GetTimerManager().SetTimer(WindupTimer, this, &ThisClass::PlayWindup, WindupDelay, false);
	else PlayWindup();
	// Keep the beat's duration even when the player closes the sound window early.
	World->GetTimerManager().SetTimer(EndTimer, this, &ThisClass::FinishWindow, Duration + 0.02f, false);
}

void UStageSoundCueAction::PlayWindup()
{
	const auto* Character = Cast<ACharacter>(StageInteraction::GetOwner(Context));
	auto* Instance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Instance || !WindupAnimation || WindupAnimation->GetPlayLength() <= 0.0f) return;
	AnimInstance = Instance;
	WindupMontage = Instance->PlaySlotAnimationAsDynamicMontage(
	    WindupAnimation, AnimationSlot, 0.07f, 0.04f, WindupAnimation->GetPlayLength() / WindupDuration);
}

void UStageSoundCueAction::HandleSoundResult(const FStageSoundWindow& Window, EStageSoundResult Result)
{
	if (Window.Id != WindowId) return;
	if (Result == EStageSoundResult::OnTime) bPlayedOnTime = true;
	if (const auto* Manager = Context.Manager.Get(); bCueShown && Manager && Manager->PlayerCues)
	{
		if (Result == EStageSoundResult::Ready) Manager->PlayerCues->SetSoundWindow(CueId, Window);
		else if (Result != EStageSoundResult::Cancelled)
		{
			Manager->PlayerCues->ReportSoundResult(CueId, Result, Window.bActive);
		}
	}
	if (Result == EStageSoundResult::Ready)
		if (UWorld* World = GetWorld()) World->GetTimerManager().SetTimer(EndTimer, this, &ThisClass::FinishWindow,
		    FMath::Max(0.02f, Window.ExpiresAt - static_cast<float>(World->GetTimeSeconds()) + 0.02f), false);
	if (bRequireOnTime && Result == EStageSoundResult::OnTime) Finish(EStageActionResult::Succeeded);
}

void UStageSoundCueAction::FinishWindow()
{
	Finish(bRequireOnTime ? EStageActionResult::Failed : EStageActionResult::Succeeded);
}

void UStageSoundCueAction::Cleanup_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WindupTimer);
		World->GetTimerManager().ClearTimer(EndTimer);
	}
	if (auto* Instance = AnimInstance.Get(); Instance && WindupMontage.IsValid())
		Instance->Montage_Stop(0.08f, WindupMontage.Get());
	if (auto* Console = Control.Get())
	{
		Console->OnCueResult.RemoveDynamic(this, &ThisClass::HandleSoundResult);
		const FStageSoundWindow Window = Console->GetSoundWindow();
		if (Window.bActive && Window.Id == WindowId) Console->CancelWindow();
	}
	if (const auto* Manager = Context.Manager.Get(); bCueShown && Manager && Manager->PlayerCues)
		Manager->PlayerCues->FinishActionCue(CueId, GetResult() == EStageActionResult::Succeeded);
}
