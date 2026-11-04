// Fill out your copyright notice in the Description page of Project Settings.


#include "Managers/Stage/StageManagerSubsystem.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Managers/Stage/StageManager.h"

bool UStageManagerSubsystem::SelectStage(const FGameplayTag& InStageTag)
{
	CheckStageManager();
	const auto StateTreeComponent = GetStateTreeComponentFromManager();
	
	if (!StateTreeComponent || 
		!InStageTag.IsValid() || 
		(CurrentStage.IsValid() && CurrentStage.StageState != EStageState::None)) 
		return false;
	
	const auto Stage = AvailableStages.FindByKey(InStageTag);
	if (Stage && Stage->StageStateTree)
	{
		CurrentStage = *Stage;
		
		StateTreeComponent->SetStateTree(CurrentStage.StageStateTree);
		StateTreeComponent->StartLogic();
		
		SetStageStage(EStageState::Active);
		OnStageStarted.Broadcast(CurrentStage);
		return true;
	}
	
	return false;
}

bool UStageManagerSubsystem::RequestStopCurrentStage()
{
	if (!CurrentStage.IsValid() || CurrentStage.StageState != EStageState::Active) return false;
	
	return SetStageStage(EStageState::PendingToStop);
}

bool UStageManagerSubsystem::RequestFinishCurrentStage()
{
	if (!CurrentStage.IsValid() || CurrentStage.StageState != EStageState::Active) return false;
	
	return SetStageStage(EStageState::PendingToFinish);
}

bool UStageManagerSubsystem::StopCurrentStage()
{
	if (!CurrentStage.IsValid() || 
		CurrentStage.StageState != EStageState::PendingToStop &&
		CurrentStage.StageState != EStageState::PendingToFinish) return false;
	
	if (const auto StateTreeComponent = GetStateTreeComponentFromManager())
	{
		StateTreeComponent->OnStateTreeRunStatusChanged.AddUniqueDynamic(this, &ThisClass::HandleStateTreeChangedStatus);
		StateTreeComponent->StopLogic("Stop Stage");
	}
	else
	{
		FinishCurrentStage();
	}
	
	return true;
}

bool UStageManagerSubsystem::SetStageStage(const EStageState& InStageState)
{
	if (!CurrentStage.IsValid() || CurrentStage.StageState == InStageState) return false;
	
	CurrentStage.StageState = InStageState;
	OnStageStateChanged.Broadcast(CurrentStage, InStageState);
	return true;
}

void UStageManagerSubsystem::FinishCurrentStage()
{
	if (!CurrentStage.IsValid()) return;
	
	FStageData LastStage = CurrentStage;
	CurrentStage = FStageData();
	LastStage.StageState = EStageState::Finished;
	OnStageStateChanged.Broadcast(LastStage, EStageState::Finished);
	OnStageStopped.Broadcast(LastStage);
}

void UStageManagerSubsystem::HandleStateTreeChangedStatus(EStateTreeRunStatus StateTreeRunStatus)
{
	if (StateTreeRunStatus == EStateTreeRunStatus::Succeeded ||
		StateTreeRunStatus == EStateTreeRunStatus::Failed ||
		StateTreeRunStatus == EStateTreeRunStatus::Stopped)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FinishCurrentStage);
	}
}

bool UStageManagerSubsystem::GetStageMemberData(const FGameplayTag& InMemberTag, FStageMemberData& MemberData)
{
	if (!CurrentStage.IsValid() || CurrentStage.StageMembers.IsEmpty() || !InMemberTag.IsValid()) return false;
	
	if (const auto MemberDataPtr = CurrentStage.StageMembers.FindByKey(InMemberTag))
	{
		MemberData = *MemberDataPtr;
		return true;
	}
	
	return false;
}

bool UStageManagerSubsystem::SetStageMemberData(const FStageMemberData& InMemberData)
{
	if (!CurrentStage.IsValid() || CurrentStage.StageMembers.IsEmpty() || !InMemberData.MemberTag.IsValid()) return false;
	
	if (auto MemberDataPtr = CurrentStage.StageMembers.FindByKey(InMemberData.MemberTag))
	{
		const bool bIsMemberStateChanged = MemberDataPtr->MemberState != InMemberData.MemberState;
		*MemberDataPtr = InMemberData;
		
		if (bIsMemberStateChanged) OnStageMemberStateChanged.Broadcast(*MemberDataPtr, MemberDataPtr->MemberState);
		
		return true;
	}
	
	return false;
}

bool UStageManagerSubsystem::AddMember(const FStageMemberData& InMemberData)
{
	if (!InMemberData.MemberTag.IsValid() || !CurrentStage.IsValid()) return false;
	
	if (CurrentStage.StageMembers.Contains(InMemberData)) return false;
	
	CurrentStage.StageMembers.Add(InMemberData);
	OnStageMemberNumberChanged.Broadcast(InMemberData, true);
	
	return true;
}

bool UStageManagerSubsystem::RemoveMember(const FGameplayTag& InMemberTag)
{
	if (!InMemberTag.IsValid() || !CurrentStage.IsValid()) return false;
	
	if (!CurrentStage.StageMembers.Contains(InMemberTag)) return false;
	
	for (int32 i = 0; i < CurrentStage.StageMembers.Num(); ++i)
	{
		const auto StageMember = CurrentStage.StageMembers[i];
		if (StageMember.MemberTag == InMemberTag)
		{
			CurrentStage.StageMembers.RemoveAtSwap(i);
			OnStageMemberNumberChanged.Broadcast(StageMember, false);
			
			return true;
		}
	}
	
	return false;
}

TObjectPtr<UStateTreeComponent> UStageManagerSubsystem::GetStateTreeComponentFromManager() const
{
	if (!StageManager.IsValid() && !StageManager.Get()) return nullptr;
	return StageManager->StateTreeComponent;
}

void UStageManagerSubsystem::CheckStageManager()
{
	if (!GetWorld()) return;
	if (StageManager.IsValid() && StageManager.Get()) return;
	
	if (TActorIterator<AStageManager> It(GetWorld(), AStageManager::StaticClass()); It)
	{
		StageManager = *It;
		AvailableStages = StageManager->GetStages();
	}
}