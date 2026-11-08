// Statuses Component by GAIDJIIN

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StatusesInfo.generated.h"

class UApplyTagLogicObject;

UENUM(BlueprintType)
enum EStatusState : uint8
{
	Constant,
	Temporary
};

USTRUCT(BlueprintType)
struct FStatusesFindInfo
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
		FGameplayTagContainer StatusesToFind;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
		bool bCheckAll = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
		bool bExactCheck = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
		bool bInverseCondition = false;
	
	FStatusesFindInfo()
	{
	}

	FStatusesFindInfo(const FGameplayTagContainer NewStatusesToFind, const bool bNewCheckAll,
		const bool bNewExactCheck, const bool bNewInverseCondition) : StatusesToFind(NewStatusesToFind),
	bCheckAll(bNewCheckAll), bExactCheck(bNewExactCheck), bInverseCondition(bNewInverseCondition)
	{
	}
};

USTRUCT(BlueprintType)
struct FStatusesInfo
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
		FGameplayTagContainer Statuses;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
		TEnumAsByte<EStatusState> StatusesState = Constant;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component",meta=(ClampMin="0.00001", EditCondition = "StatusesState==EStatusState::Temporary", EditConditionHides))
		float TemporaryTimeStatus = 0.00001f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component", meta=(EditCondition = "StatusesState==EStatusState::Temporary", EditConditionHides))
		bool bIsClearTimer = false;

	FStatusesInfo()
	{
	}

	FStatusesInfo(const FGameplayTagContainer NewStatuses, TEnumAsByte<EStatusState> NewStatusesState = Constant,
	              const float NewTemporaryTimeStatus = 0.00001) :
		Statuses(NewStatuses), StatusesState(NewStatusesState),
		TemporaryTimeStatus(FMath::Max(0.00001f, NewTemporaryTimeStatus))
	{
	}
};

USTRUCT(BlueprintType)
struct FStatusesInfoArray
{
	GENERATED_USTRUCT_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
		TArray<FStatusesInfo> StatusesInfo;

	FStatusesInfoArray()
	{
	}

	FStatusesInfoArray(const TArray<FStatusesInfo>& NewStatuses) : StatusesInfo(NewStatuses)
	{
	}
};