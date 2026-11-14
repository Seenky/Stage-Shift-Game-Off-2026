// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "StageData.generated.h"

class AActor;
class AStageManager;
class UStageAction;
class UStageMemberCommand;
class UStageMemberComponent;

// Tags
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stage_Member);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stage_MemberState);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stage_Scene);

// Enums

UENUM(BlueprintType)
enum class EStageState : uint8
{
	Idle,
	Running,
	Finalizing,
	Stopping
};

UENUM(BlueprintType)
enum class EStageExecutionState : uint8
{
	NotStarted, // None
	Running, // Working
	Stopping, // Stop requested
	Finished // Finished
};

UENUM(BlueprintType)
enum class EStageCommandResult : uint8
{
	None,
	Succeeded,
	Skipped,
	Failed,
	Cancelled,
	Blocked
};

UENUM(BlueprintType)
enum class EStageActionResult : uint8
{
	None,
	Succeeded,
	Failed,
	Interrupted,
	Cancelled
};

UENUM(BlueprintType)
enum class EStageCommandType : uint8
{
	Scenario,
	Reaction,
	Finale
};

UENUM(BlueprintType)
enum class EStageLatenessLevel : uint8
{
	OnTime,
	Easy,
	Medium,
	Hard
};

UENUM(BlueprintType)
enum class EStageCatchUpPolicy : uint8
{
	Keep,
	UseShortIfNeed,
	SkipBeforeStart
};

UENUM(BlueprintType)
enum class EStageActionStopReason : uint8
{
	Interrupt,
	Cancel
};

UENUM(BlueprintType)
enum class EStageActionStopMode : uint8
{
	StopAction,
	FinishAction
};

// Structs

// Member Command
USTRUCT(BlueprintType)
struct FStageMemberAssignment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTag MemberTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Stage")
	TObjectPtr<UStageMemberCommand> Command = nullptr;
};

USTRUCT(BlueprintType)
struct FStageStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	TArray<FStageMemberAssignment> Commands;
};

USTRUCT(BlueprintType)
struct FStageExecutionRequirements
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTagContainer DenyTags;
	
	bool GetIsAccess(const FGameplayTagContainer& InTags) const
	{
		if (RequiredTags.IsEmpty() && DenyTags.IsEmpty()) return true;
		
		return InTags.HasAll(RequiredTags) && !InTags.HasAny(DenyTags);
	}
};

// Rule for actions
USTRUCT(BlueprintType)
struct FStageActionRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Stage")
	TObjectPtr<UStageAction> Action = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FStageExecutionRequirements ExecRequirements;
};

USTRUCT(BlueprintType)
struct FStageMemberReaction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTag SubjectRoleTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTag MemberTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Stage")
	TObjectPtr<UStageMemberCommand> Command = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FStageExecutionRequirements ExecRequirements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage", meta = (ToolTip = "Temporary status applied to the reacting actor at reaction start. Leave empty to disable cooldown."))
	FGameplayTag CooldownTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float CooldownDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage", meta = (ClampMin = "0", UIMin = "0", ToolTip = "Zero means no activation limit for this Stage."))
	int32 MaxActivationsPerStage = 0;
};

USTRUCT(BlueprintType)
struct FStageEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	TObjectPtr<AActor> SubjectActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	TObjectPtr<AActor> InstigatorActor = nullptr;
};

USTRUCT(BlueprintType)
struct FStageExecutionContext
{
	GENERATED_BODY()

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	TWeakObjectPtr<AStageManager> Manager;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	TWeakObjectPtr<UStageMemberComponent> MemberComponent;
};

USTRUCT(BlueprintType)
struct FStageRootExecution
{
	GENERATED_BODY()

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	TObjectPtr<UStageMemberCommand> Command = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	FGameplayTag MemberTag;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	TWeakObjectPtr<UStageMemberComponent> MemberComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	EStageCommandType CommandType = EStageCommandType::Scenario;
	
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	int32 SourceIndex = INDEX_NONE;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	bool bStarted = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	EStageCommandResult Result = EStageCommandResult::None;
};

USTRUCT(BlueprintType)
struct FStageCommandExecution
{
	GENERATED_BODY()

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	TObjectPtr<UStageMemberCommand> Command = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	TObjectPtr<UStageAction> CurrentMainAction = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	float RecoveryWaitDeadline = -1.0f;
};
