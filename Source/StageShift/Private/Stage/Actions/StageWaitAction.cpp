// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Actions/StageWaitAction.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "StageInteractionHelpers.h"

void UStageWaitAction::SetupWait(float InTime, UAnimSequenceBase* InGesture)
{
	WaitTime = InTime;
	Gesture = InGesture;
}

void UStageWaitAction::OnStart_Implementation()
{
	if (!GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	if (WaitTime <= 0.f)
	{
		Finish(EStageActionResult::Succeeded);
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(WaitTimerHandle, this, &ThisClass::FinishTimer, WaitTime);
	if (Gesture && Gesture->GetPlayLength() > 0.0f)
	{
		const auto* Character = Cast<ACharacter>(StageInteraction::GetOwner(Context));
		auto* Instance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if (Instance)
		{
			GestureInstance = Instance;
			GestureMontage = Instance->PlaySlotAnimationAsDynamicMontage(Gesture, TEXT("RightHand"), 0.08f, 0.12f, Gesture->GetPlayLength() / WaitTime);
		}
	}
}

void UStageWaitAction::OnStop_Implementation(EStageActionStopMode Mode)
{
	if (!GetWorld())
	{
		Finish(EStageActionResult::Failed);
		return;
	}

	WaitTime = GetWorld()->GetTimerManager().GetTimerRemaining(WaitTimerHandle);
	if (Mode == EStageActionStopMode::StopAction)
	{
		GetWorld()->GetTimerManager().ClearTimer(WaitTimerHandle);
		Finish(EStageActionResult::Interrupted);
	}
}

void UStageWaitAction::Cleanup_Implementation()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(WaitTimerHandle);
	if (auto* Instance = GestureInstance.Get(); Instance && GestureMontage.IsValid()) Instance->Montage_Stop(0.1f, GestureMontage.Get());
}

void UStageWaitAction::FinishTimer()
{
	Finish(EStageActionResult::Succeeded);
}
