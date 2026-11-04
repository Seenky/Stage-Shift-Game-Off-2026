// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StateTree.h"
#include "Templates/TypeHash.h"
#include "UObject/Object.h"
#include "StageData.generated.h"

// Tags
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stage_Member);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stage_MemberState);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stage_Scene);

USTRUCT(BlueprintType)
struct FStageMemberData
{
	GENERATED_BODY()
	
	FStageMemberData() = default;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName CurrentName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Stage.Member"))
	FGameplayTag MemberTag = FGameplayTag::EmptyTag;
	
	UPROPERTY(BlueprintReadWrite, meta=(Categories="Stage.MemberState"))
	FGameplayTag MemberState = FGameplayTag::EmptyTag;
	
	bool operator==(const FStageMemberData& Other) const
	{
		return MemberTag == Other.MemberTag;
	}
	
	bool operator==(const FGameplayTag& OtherTag) const
	{
		return MemberTag == OtherTag;
	}
	
	friend uint32 GetTypeHash(const FStageMemberData& Arg)
	{
		return GetTypeHash(Arg.MemberTag);
	}
};

UENUM(BlueprintType)
enum class EStageState : uint8
{
	None,
	Active,
	PendingToFinish,
	PendingToStop,
	Finished
};

USTRUCT(BlueprintType)
struct FStageData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName StageName = NAME_None;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(Categories="Stage.Scene"))
	FGameplayTag StageTag = FGameplayTag::EmptyTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FStageMemberData> StageMembers = {};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStateTree> StageStateTree = nullptr;
	
	UPROPERTY(BlueprintReadWrite)
	EStageState StageState = EStageState::None;
	
	bool operator==(const FStageData& Other) const
	{
		return StageTag == Other.StageTag;
	}
	
	bool operator==(const FGameplayTag& OtherTag) const
	{
		return StageTag == OtherTag;
	}
	
	friend uint32 GetTypeHash(const FStageData& Arg)
	{
		return GetTypeHash(Arg.StageTag);
	}
	
	bool IsValid() const
	{
		return StageTag.IsValid() && StageStateTree;
	}
};