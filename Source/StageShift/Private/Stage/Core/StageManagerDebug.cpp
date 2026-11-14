#include "Stage/Core/StageManager.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageAction.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StatusesComponent.h"

namespace
{
	TAutoConsoleVariable<int32> CVarStageDebug(
		TEXT("Stage.Debug"), 0,
		TEXT("Show StageManager on-screen debug: 1 = on, 0 = off."), ECVF_Cheat);

	template <typename T>
	FString EnumName(T Value)
	{
		return StaticEnum<T>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	FString ActionDescription(const UStageAction* Action)
	{
		if (!IsValid(Action)) return TEXT("none");
		return FString::Printf(TEXT("%s [%s / %s]"), *Action->GetClass()->GetName(),
			*EnumName(Action->GetState()), *EnumName(Action->GetResult()));
	}
}

void AStageManager::UpdateDebugDisplay() const
{
	if (!GEngine) return;
	const uint64 MessageKey = 0x5354414700000000ull | static_cast<uint64>(GetUniqueID());
	if (CVarStageDebug.GetValueOnGameThread() == 0)
	{
		GEngine->RemoveOnScreenDebugMessage(MessageKey);
		return;
	}

	FString Text = FString::Printf(TEXT("STAGE DEBUG | %s | %s\nScore: %d | Stars: %d\n"),
		*GetActorNameOrLabel(), *EnumName(State), Score, GetStarCount());
	Text += FString::Printf(TEXT("Lateness: %.2fs | Max: %.2fs | %s%s\n"),
		CurrentLatenessSeconds, MaxLatenessSeconds, *EnumName(CurrentLatenessLevel),
		State == EStageState::Finalizing || State == EStageState::Stopping ? TEXT(" (frozen)") : TEXT(""));

	if (IsValid(ActiveStage))
	{
		const float Elapsed = GetStageElapsedTime();
		Text += FString::Printf(TEXT("Stage: %s | %s\nTime: %.2f / %.2fs | Deadline in: %.2fs | Tolerance: %.2fs\n"),
			*ActiveStage->StageId.ToString(), *ActiveStage->GetName(), Elapsed,
			ActiveStage->MainDuration, FMath::Max(0.0f, ActiveStage->MainDuration - Elapsed), ActiveStage->AdditiveDuration);

		int32 FinishedAssignments = 0;
		for (const auto& Pair : CurrentStepResults)
			if (Pair.Value != EStageCommandResult::None) ++FinishedAssignments;
		Text += FString::Printf(TEXT("Step: %d / %d | Assignments finished: %d / %d\n"),
			CurrentStepIndex + 1, ActiveStage->Steps.Num(), FinishedAssignments, CurrentStepResults.Num());

		int32 FinaleIssued = 0;
		int32 FinaleFinished = 0;
		int32 FinaleSucceeded = 0;
		int32 OldWorkPending = 0;
		for (const FStageRootExecution& Entry : RootExecutions)
		{
			if (Entry.CommandType == EStageCommandType::Finale)
			{
				++FinaleIssued;
				if (Entry.Result != EStageCommandResult::None) ++FinaleFinished;
				if (Entry.Result == EStageCommandResult::Succeeded) ++FinaleSucceeded;
			}
			else if (Entry.Result == EStageCommandResult::None) ++OldWorkPending;
		}
		if (State == EStageState::Running)
			Text += FString::Printf(TEXT("Finale: not requested (%d assignments); starts after Steps or deadline\n"), ActiveStage->Finale.Num());
		else
			Text += FString::Printf(TEXT("Finale: %d / %d finished | %d succeeded | Old assignments pending: %d\n"),
				FinaleFinished, FinaleIssued, FinaleSucceeded, OldWorkPending);
	}
	else Text += FString::Printf(TEXT("No active Stage | Last elapsed time: %.2fs\n"), StageElapsedTime);
	if (!FailureReason.IsEmpty()) Text += FString::Printf(TEXT("Failure: %s\n"), *FailureReason.ToString());

	TArray<FGameplayTag> Roles;
	Members.GetKeys(Roles);
	Roles.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.ToString() < B.ToString(); });
	Text += FString::Printf(TEXT("Members: %d\n"), Roles.Num());
	for (const FGameplayTag& MemberTag : Roles)
	{
		const AActor* Actor = GetMember(MemberTag);
		const UStageMemberComponent* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
		const UStatusesComponent* Statuses = IsValid(Actor) ? Actor->FindComponentByClass<UStatusesComponent>() : nullptr;
		const FString ActorName = IsValid(Actor) ? Actor->GetActorNameOrLabel() : TEXT("unavailable");
		Text += FString::Printf(TEXT("\n%s -> %s\n  Statuses: %s\n"), *MemberTag.ToString(), *ActorName,
			Statuses ? *Statuses->GetStatuses().ToStringSimple() : TEXT("none"));
		if (!Member) continue;

		TFunction<void(UStageMemberCommand*, const FString&, int32)> DescribeCommand;
		DescribeCommand = [&](UStageMemberCommand* Command, const FString& Path, int32 Depth)
		{
			if (!IsValid(Command)) return;
			const FString Indent = FString::ChrN(Depth * 2 + 2, TEXT(' '));
			Text += FString::Printf(TEXT("%s%s %s [%s / %s] | Remaining: %.2fs%s\n"),
				*Indent, *Path, *Command->GetClass()->GetName(), *EnumName(Command->GetState()),
				*EnumName(Command->GetResult()), Command->GetRemainingTime(),
				Command->IsUsingShortVersion() ? TEXT(" | SHORT") : TEXT(""));
			if (!Command->IsComposite())
			{
				const UStageAction* Main = Member->GetMainAction(Command);
				Text += FString::Printf(TEXT("%s  Main Action: %s\n"), *Indent, *ActionDescription(Main));
				if (!Main && Command->GetState() == EStageExecutionState::Running)
				{
					const bool bRequirementsMet = Statuses && Command->ExecutionRequirements.GetIsAccess(Statuses->GetStatuses());
					const TCHAR* WaitingFor = Member->GetRecoveryAction() ? TEXT("RecoveryAction") :
						(bRequirementsMet ? TEXT("execution update") : TEXT("RequiredTags / DenyTags"));
					Text += FString::Printf(TEXT("%s  Waiting for: %s\n"), *Indent, WaitingFor);
				}
			}
			const auto& Children = Command->GetChildCommands();
			for (int32 Index = 0; Index < Children.Num(); ++Index)
				DescribeCommand(Children[Index], FString::Printf(TEXT("%s.%d"), *Path, Index + 1), Depth + 1);
		};

		bool bHasVisibleAssignment = false;
		for (const FStageRootExecution& Entry : RootExecutions)
		{
			if (Entry.MemberTag != MemberTag) continue;
			const bool bCurrentStep = Entry.CommandType == EStageCommandType::Scenario && Entry.SourceIndex == CurrentStepIndex;
			if (!bCurrentStep && Entry.CommandType != EStageCommandType::Finale && Entry.Result != EStageCommandResult::None) continue;
			bHasVisibleAssignment = true;
			const FString Assignment = Entry.CommandType == EStageCommandType::Scenario
				? FString::Printf(TEXT("Scenario Step %d"), Entry.SourceIndex + 1) : EnumName(Entry.CommandType);
			Text += FString::Printf(TEXT("  %s\n"), *Assignment);
			DescribeCommand(Entry.Command, TEXT("1"), 1);
		}
		if (!bHasVisibleAssignment) Text += TEXT("  No current assignment\n");
		Text += FString::Printf(TEXT("  Recovery Action: %s\n"), *ActionDescription(Member->GetRecoveryAction()));
	}

	GEngine->AddOnScreenDebugMessage(MessageKey, 0.25f, FColor::Cyan, Text, false);
}
