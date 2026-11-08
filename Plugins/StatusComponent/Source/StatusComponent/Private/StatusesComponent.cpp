// Statuses Component by GAIDJIIN


#include "StatusesComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Net/UnrealNetwork.h"


void UStatusesComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                       FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if !UE_BUILD_SHIPPING
	ShowDebug();
#endif
}

void UStatusesComponent::BeginPlay()
{
	Super::BeginPlay();

	AddConstantStatuses(StartStatuses);
}

UStatusesComponent::UStatusesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStatusesComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Replicate statuses
	DOREPLIFETIME(UStatusesComponent, Statuses);
	DOREPLIFETIME(UStatusesComponent, TemporaryTags);
	DOREPLIFETIME(UStatusesComponent, CurrentActiveStatus);
}

// Getter Info

bool UStatusesComponent::GetStatusesInfo(const FGameplayTagContainer& StatusesToGet,
                                         TArray<FStatusesInfo>& ReturnStatuses) const
{
	if (StatusesToGet.IsEmpty()) return false;
	auto LocalStatuses = StatusesToGet.GetGameplayTagArray();
	bool bLocalResult = false;
	for (auto LocalStatus : LocalStatuses)
	{
		FStatusesInfo LocalReturnStatusInfo;
		const bool bLocalSuccess = GetStatusInfo(LocalStatus, LocalReturnStatusInfo);
		if (bLocalSuccess)
		{
			ReturnStatuses.Add(LocalReturnStatusInfo);
			if (!bLocalResult) bLocalResult = true;
		}
	}
	return bLocalResult;
}

bool UStatusesComponent::GetStatusInfo(const FGameplayTag& StatusToGet, FStatusesInfo& ReturnStatusInfo) const
{
	if (!StatusToGet.IsValid() || !GetWorld()) return false;
	TEnumAsByte<EStatusState> LocalStatusState;
	if (!GetStatusState(StatusToGet, LocalStatusState)) return false;
	switch (LocalStatusState)
	{
	case Constant:
		{
			if (!GetIsContainStatus(StatusToGet)) return false;
			ReturnStatusInfo = FStatusesInfo(StatusToGet.GetSingleTagContainer());
			break;
		}
	case Temporary:
		{
			const auto LocalTemporaryStatus = TemporaryTags.FindByPredicate([&StatusToGet](const FTemporaryStatusInfo& A)
			{
				return A.TemporaryStatus == StatusToGet;
			});
			
			if(!LocalTemporaryStatus) return false;
			
			const float LocalRemainingTimeStatus = GetWorld()->GetTimerManager().GetTimerRemaining(
				LocalTemporaryStatus->TemporaryStatusTimerHandle);
			
			ReturnStatusInfo = FStatusesInfo(
				StatusToGet.GetSingleTagContainer(),
				Temporary,
				LocalRemainingTimeStatus
			);
			break;
		}
	default:
		break;
	}
	return true;
}

bool UStatusesComponent::GetStatusState(const FGameplayTag& StatusToCheck, TEnumAsByte<EStatusState>& StatusState) const
{
	if (!GetIsContainStatus(StatusToCheck)) return false;
	StatusState = Constant;
	if (TemporaryTags.Contains(FTemporaryStatusInfo(StatusToCheck, FTimerHandle()))) StatusState = Temporary;
	return true;
}

bool UStatusesComponent::GetStatusesInfoByState(const TEnumAsByte<EStatusState> StatusState,
	TArray<FStatusesInfo>& ReturnStatusesInfo) const
{
	if(Statuses.IsEmpty()) return false;
	TArray<FStatusesInfo> LocalReturnStatuses;
	if(!GetStatusesInfo(Statuses,LocalReturnStatuses)) return false;
	ReturnStatusesInfo.Empty();
	ReturnStatusesInfo = LocalReturnStatuses.FilterByPredicate([&](const FStatusesInfo LocalStatus)
	{ return LocalStatus.StatusesState == StatusState; });
	return !ReturnStatusesInfo.IsEmpty();
}

bool UStatusesComponent::GetStatusesByState(const TEnumAsByte<EStatusState> StatusState,
	FGameplayTagContainer& ReturnStatuses) const
{
	if(Statuses.IsEmpty()) return false;
	TArray<FStatusesInfo> LocalReturnStatuses;
	if(!GetStatusesInfo(Statuses,LocalReturnStatuses)) return false;
	for(const auto LocalStatus : LocalReturnStatuses)
	{
		if(LocalStatus.StatusesState != StatusState) continue;
		ReturnStatuses.AppendTags(LocalStatus.Statuses);
	}
	return !ReturnStatuses.IsEmpty();
}

bool UStatusesComponent::GetAllStatusesInfo(TArray<FStatusesInfo>& ReturnStatusesInfo) const
{
	const auto LocalStatusesInfo = GetStatusesInfo(Statuses, ReturnStatusesInfo);
	return LocalStatusesInfo;
}

bool UStatusesComponent::GetIsContainStatus(const FGameplayTag& StatusToFind, const bool ExactCheck,
                                            const bool InverseCondition) const
{
	const bool Result = ExactCheck ? Statuses.HasTagExact(StatusToFind) : Statuses.HasTag(StatusToFind);
	return InverseCondition ? !Result : Result;
}

bool UStatusesComponent::GetIsContainStatuses(const FGameplayTagContainer& StatusesToFind, const bool CheckAll,
                                              const bool ExactCheck, const bool InverseCondition) const
{
	const bool ResultExactCheck =
		CheckAll ? Statuses.HasAllExact(StatusesToFind) : Statuses.HasAnyExact(StatusesToFind);
	const bool ResultNotExactCheck = CheckAll ? Statuses.HasAll(StatusesToFind) : Statuses.HasAny(StatusesToFind);
	if (ExactCheck) return InverseCondition ? !ResultExactCheck : ResultExactCheck;
	return InverseCondition ? !ResultNotExactCheck : ResultNotExactCheck;
}

// Main Add Statuses Logic

void UStatusesComponent::AddConstantStatuses_Implementation(const FGameplayTagContainer& ConstantStatuses)
{
	AddStatuses(ConstantStatuses);
}

void UStatusesComponent::ChangeOrAddCurrentActiveStatus_Implementation(const FGameplayTag& NewCurrentActiveStatus, bool bIsChangeActiveStatus)
{
	
	// Remove old current active status
	if(!GetIsEqualCurrentActiveStatus(NewCurrentActiveStatus) && bIsChangeActiveStatus) RemoveActiveStatus();

	// Add new current active status
	AddConstantStatuses(NewCurrentActiveStatus.GetSingleTagContainer());

	CurrentActiveStatus = NewCurrentActiveStatus;

	OnChangeCurrentActiveStatus.Broadcast(CurrentActiveStatus);
}

void UStatusesComponent::AddStatusesWithInfo_Implementation(const FStatusesInfoArray& StatusesToAdd)
{
	if (StatusesToAdd.StatusesInfo.IsEmpty()) return;
	bool bLocalResult = false;
	
	// Filter Get Can added tag container for call delegate added tags
	TArray<FGameplayTag> LocalOldStatusesArray;
	Statuses.GetGameplayTagArray(LocalOldStatusesArray);
	
	for (auto StatusContainerToAdd : StatusesToAdd.StatusesInfo)
	{
		if (StatusContainerToAdd.Statuses.IsEmpty()) continue;
		bool bLocalFlag = false;
		switch (StatusContainerToAdd.StatusesState)
		{
		case Constant:
			bLocalFlag = AddStatuses(StatusContainerToAdd.Statuses);
			break;
		case Temporary:
			bLocalFlag = AddTemporaryStatuses(StatusContainerToAdd.Statuses, StatusContainerToAdd.TemporaryTimeStatus,
			                     StatusContainerToAdd.bIsClearTimer);
			break;
		default:
			break;
		}
		if(bLocalFlag) bLocalResult = true;
	}

	// Check New Gameplay Tag
	TArray<FGameplayTag> LocalNewStatusesArray;
	Statuses.GetGameplayTagArray(LocalNewStatusesArray);
	const auto LocalAddedStatuses = FGameplayTagContainer::CreateFromArray(
		LocalNewStatusesArray.FilterByPredicate([&](const FGameplayTag LocalFindedTag)
		{
			return !LocalOldStatusesArray.Contains(LocalFindedTag);
		})
		);
	
	if (bLocalResult && !LocalAddedStatuses.IsEmpty()) OnAddStatuses.Broadcast(LocalAddedStatuses);
}

bool UStatusesComponent::AddStatus(const FStatusesInfo& StatusToAdd)
{
	if (GetIsContainStatuses(StatusToAdd.Statuses, true) || !StatusToAdd.Statuses.IsValid()) return false;
	Statuses.AppendTags(StatusToAdd.Statuses);
	return true;
}

bool UStatusesComponent::AddStatuses(const FGameplayTagContainer& StatusesToAdd)
{
	if (GetIsContainStatuses(StatusesToAdd, true, true) || !StatusesToAdd.IsValid()) return false;
	for (auto Tag : StatusesToAdd)
	{
		FStatusesInfo LocalAddTag(Tag.GetSingleTagContainer());
		AddStatus(LocalAddTag);
	}
	return true;
}

void UStatusesComponent::RemoveActiveStatus_Implementation()
{
	RemoveStatuses(CurrentActiveStatus.GetSingleTagContainer());
}

void UStatusesComponent::RemoveStatuses_Implementation(const FGameplayTagContainer& StatusesToRemove)
{
	if (!GetIsContainStatuses(StatusesToRemove, false, true) || !StatusesToRemove.IsValid()) return;
	FGameplayTagContainer RemovedTag;
	for (auto Tag : StatusesToRemove)
	{
		if (!GetIsContainStatus(Tag)) continue;
		RemovedTag.AddTag(Tag);
		ClearTemporaryStatusTimer(Tag);
	}

	// If Statuses to remove has active state - remove status
	if(StatusesToRemove.HasTag(CurrentActiveStatus)) CurrentActiveStatus = FGameplayTag();
	
	if (RemovedTag.IsEmpty()) return;
	Statuses.RemoveTags(RemovedTag);
	OnRemoveStatuses.Broadcast(RemovedTag);

}

void UStatusesComponent::RemoveAllStatuses_Implementation()
{ return RemoveStatuses(Statuses); }

// Temporary Logic

bool UStatusesComponent::AddTemporaryStatuses(const FGameplayTagContainer& StatusesToAdd,
                                              const float TimeToDeleteStatuses, const bool bClearTimer)
{
	if (!GetWorld() || !StatusesToAdd.IsValid()) return false;
	bool bLocalResult = false;
	for (auto Tag : StatusesToAdd)
	{
		const bool bLocalFlag = MakeTemporaryStatus(Tag, TimeToDeleteStatuses, bClearTimer);
		if (bLocalFlag) bLocalResult = true;
	}
	return bLocalResult;
}

bool UStatusesComponent::ClearTemporaryStatusTimer(const FGameplayTag& StatusToClear)
{
	if (!TemporaryTags.Contains(FTemporaryStatusInfo(StatusToClear, FTimerHandle())) || !GetWorld()) return false;

	auto LocalTemporaryStatus = *TemporaryTags.FindByPredicate([&StatusToClear](const FTemporaryStatusInfo& A)
	{
		return A.TemporaryStatus == StatusToClear;
	});
	
	GetWorld()->GetTimerManager().ClearTimer(LocalTemporaryStatus.TemporaryStatusTimerHandle);
	TemporaryTags.Remove(LocalTemporaryStatus);
	
	return true;
}

bool UStatusesComponent::MakeTemporaryStatus(const FGameplayTag& StatusToAdd, const float TimeToDeleteStatus,
                                             const bool bClearTimer)
{
	if(!GetWorld()) return false;
	const FStatusesInfo LocalAddTempTag(StatusToAdd.GetSingleTagContainer(),EStatusState::Temporary,TimeToDeleteStatus);
	AddStatus(LocalAddTempTag);
	if (!bClearTimer && TemporaryTags.Contains(StatusToAdd)) return false;
	ClearTemporaryStatusTimer(StatusToAdd);
	FTimerHandle LocalHandle;
	FTimerDelegate TemporaryTagDelegate;
	TemporaryTagDelegate.BindUFunction(this, "RemoveStatuses", StatusToAdd.GetSingleTagContainer());
	GetWorld()->GetTimerManager().SetTimer(LocalHandle, TemporaryTagDelegate, TimeToDeleteStatus, false);
	
	TemporaryTags.AddUnique(FTemporaryStatusInfo(StatusToAdd, LocalHandle));
	
	return true;
}

// Debug Logic

#if !UE_BUILD_SHIPPING
	void UStatusesComponent::ShowDebug()
	{
		if(!GetWorld() || !bShowDebug) return;

		// Show Constant Statuses
		FGameplayTagContainer LocalConstantStatuses;
		GetStatusesByState(Constant,LocalConstantStatuses);
		FString DebugStringConstantTags = UKismetStringLibrary::JoinStringArray(
			UKismetStringLibrary::ParseIntoArray(LocalConstantStatuses.ToStringSimple(), ","), "\n");

		// Show Temporary Statuses
		const auto TemporaryTagsKeys = TemporaryTags;
		
		FString DebugStringTemporaryTags;
		for (auto TemporaryTagInfo : TemporaryTagsKeys) DebugStringTemporaryTags += TemporaryTagInfo.TemporaryStatus.ToString()
		+ " : " + FString::SanitizeFloat(GetWorld()->GetTimerManager().GetTimerRemaining(TemporaryTagInfo.TemporaryStatusTimerHandle)) + '\n';
		
		UKismetSystemLibrary::PrintString(
			GetWorld(),
			"Statuses Info:\n--------------------------------\n" + DebugStringConstantTags + "\n" + DebugStringTemporaryTags +
			"Current Active Status : " + CurrentActiveStatus.ToString() + "\n--------------------------------\n",
			true,
			true,
			FLinearColor::Red,
			0.0f
		);
	}
#endif

const FText UStatusesComponent::GetStatusesReadableText(const FGameplayTagContainer StatusesToText) const
{
	return FText::FromString(UKismetStringLibrary::JoinStringArray(
		UKismetStringLibrary::ParseIntoArray(StatusesToText.ToStringSimple(), ","), "\n"));
}


// Validate Methods

bool UStatusesComponent::RemoveActiveStatus_Validate()
{
	return true;
}

bool UStatusesComponent::ChangeOrAddCurrentActiveStatus_Validate(const FGameplayTag& NewCurrentActiveStatus, bool bIsChangeActiveStatus)
{
	return true;
}

bool UStatusesComponent::AddConstantStatuses_Validate(const FGameplayTagContainer& ConstantStatuses)
{
	return true;
}

bool UStatusesComponent::RemoveAllStatuses_Validate()
{
	return true;
}

bool UStatusesComponent::RemoveStatuses_Validate(const FGameplayTagContainer& StatusesToRemove)
{
	return true;
}

bool UStatusesComponent::AddStatusesWithInfo_Validate(const FStatusesInfoArray& StatusesToAdd)
{
	return true;
}