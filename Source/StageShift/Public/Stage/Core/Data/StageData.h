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
enum class EStageTimingPolicy : uint8
{
	CompleteOrDeadline UMETA(DisplayName = "Finish when complete or at the deadline"),
	FixedMainDuration UMETA(DisplayName = "Keep the scene open for the full main duration")
};

// Editor organization only. Members identify canonical steps/commands, never an execution copy.
USTRUCT()

struct FStageGraphGroup
{
	GENERATED_BODY()

	UPROPERTY() FGuid Id;
	UPROPERTY() FGuid Scope;
	UPROPERTY() FGuid Parent;
	UPROPERTY() FString Title;
	UPROPERTY() TArray<FGuid> Members;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (GetOptions = "GetRoleOptions"))
	FName RoleId;

	FName GetEffectiveRoleId() const { return RoleId.IsNone() ? MemberTag.GetTagName() : RoleId; }

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	FGameplayTag MemberTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Stage")
	TObjectPtr<UStageMemberCommand> Command = nullptr;
};

USTRUCT(BlueprintType)

struct FStageStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario")
	FName StepId;

	UPROPERTY()
	FName ThenStepId;

	// Secondary exit: On Failed for a Step, False for a Branch.
	UPROPERTY()
	FName FailedStepId;

	UPROPERTY(Instanced)
	TObjectPtr<class UStageBranch> Branch;
#if WITH_EDITORONLY_DATA
	UPROPERTY() FGuid EditorGuid;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (TitleProperty = "RoleId"))
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

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	FStageExecutionRequirements ExecRequirements;
};

USTRUCT(BlueprintType)

struct FStageMemberReaction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions")
	FName ReactionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions", meta = (GetOptions = "GetRoleOptions"))
	FName RoleId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions", meta = (GetOptions = "GetRoleOptions"))
	FName SubjectRoleId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions",
	    meta = (ToolTip = "Step IDs during which this reaction may start. An empty list allows any step. Running reactions are not stopped when the step changes."))
	TArray<FName> AllowedStepIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions")
	bool bUseCooldown = false;

	FName GetEffectiveRoleId() const { return RoleId.IsNone() ? MemberTag.GetTagName() : RoleId; }

	FName GetEffectiveSubjectRoleId() const { return SubjectRoleId.IsNone() ? SubjectRoleTag.GetTagName() : SubjectRoleId; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FGameplayTag EventTag;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	FGameplayTag SubjectRoleTag;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	FGameplayTag MemberTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Stage")
	TObjectPtr<UStageMemberCommand> Command = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reactions", meta = (TitleProperty = "RoleId"))
	TArray<FStageMemberAssignment> AdditionalPerformers;

	TArray<FStageMemberAssignment> GetAssignments() const
	{
		FStageMemberAssignment Primary;
		Primary.RoleId = GetEffectiveRoleId();
		Primary.Command = Command;
		TArray<FStageMemberAssignment> Result;
		Result.Add(Primary);
		Result.Append(AdditionalPerformers);
		return Result;
	}
#if WITH_EDITORONLY_DATA
	UPROPERTY() FGuid EditorGuid;
#endif

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	FStageExecutionRequirements ExecRequirements;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced",
	    meta = (ToolTip = "Temporary status applied to the reacting actor at reaction start. Leave empty to disable cooldown."))
	FGameplayTag CooldownTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s", EditCondition = "bUseCooldown"))
	float CooldownDuration = 5.0f;

	UPROPERTY(
	    AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced", meta = (ClampMin = "0", UIMin = "0", ToolTip = "Zero means no activation limit for this Stage."))
	int32 MaxActivationsPerStage = 0;
};

// Editor-only users of these records keep unconnected authoring work out of runtime arrays.
UENUM()
enum class EStageGraphDraftKind : uint8
{
	Step,
	Child,
	Assignment,
	Reaction
};

USTRUCT()

struct FStageGraphDraft
{
	GENERATED_BODY()

	UPROPERTY() FGuid Id;
	UPROPERTY() FGuid Scope;
	UPROPERTY() FGuid Group;
	UPROPERTY() EStageGraphDraftKind Kind = EStageGraphDraftKind::Step;
	UPROPERTY(EditAnywhere, Category = "Draft") FStageStep Step;
	UPROPERTY(EditAnywhere, Category = "Draft") FStageMemberReaction Reaction;

	UStageMemberCommand* GetCommand() const
	{
		return Kind == EStageGraphDraftKind::Reaction ? Reaction.Command.Get() : Step.Commands.IsEmpty() ? nullptr : Step.Commands[0].Command.Get();
	}
};

USTRUCT()

struct FStageGraphViewState
{
	GENERATED_BODY()

	UPROPERTY() FVector2D Location = FVector2D::ZeroVector;
	UPROPERTY() float Zoom = 0.8f;
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
	FName RoleId;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	TObjectPtr<UStageMemberCommand> Command = nullptr;

	UPROPERTY(AdvancedDisplay, Transient, BlueprintReadOnly, Category = "Advanced")
	FGameplayTag MemberTag;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	TWeakObjectPtr<UStageMemberComponent> MemberComponent;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	EStageCommandType CommandType = EStageCommandType::Scenario;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Stage|Runtime")
	int32 SourceIndex = INDEX_NONE;
	UPROPERTY(Transient) FGuid ReactionActivationId;

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
	int32 RetriesScheduled = 0;
	double RetryAt = -1.0;
	FString LastWaitReason;
};
