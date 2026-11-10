#include "Tests/StageMemberCommandLifecycleTestCommand.h"

void UStageMemberCommandLifecycleTestCommand::RecordFinished(UStageMemberCommand* Command, EStageCommandResult InResult)
{
	++NotificationCount;
	StateDuringNotification = GetState();
	Complete(EStageCommandResult::Failed); // A nested completion must be ignored.
}

float UStageMemberCommandLifecycleTestCommand::GetNeededTime_Implementation() const
{
	return 10.0f;
}

float UStageMemberCommandLifecycleTestCommand::GetRemainingTime_Implementation() const
{
	if (GetState() == EStageExecutionState::Finished) return 0.0f;
	return FMath::Max(0.0f, (IsUsingShortVersion() ? 3.0f : GetNeededTime()) - GetAccumulatedActiveTime());
}

void UStageMemberCommandLifecycleTestCommand::BeginWork_Implementation()
{
	++BeginCount;
	if (bFinishDuringBegin) Complete(EStageCommandResult::Succeeded);
}

void UStageMemberCommandLifecycleTestCommand::StopWork_Implementation(EStageActionStopReason Reason)
{
	++StopCount;
	if (!bDeferStop) Complete(EStageCommandResult::Cancelled);
}

void UStageMemberCommandLifecycleTestCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult)
{
	LastActionResult = ActionResult;
}

void UStageMemberCommandLifecycleTestCommand::ResetProgress_Implementation()
{
	PlaybackPosition = 0.0f;
	LastActionResult = EStageActionResult::None;
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "Managers/Stage/StageManager.h"
#include "Misc/AutomationTest.h"
#include "Tests/StageActionLifecycleTestAction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStageMemberCommandLifecycleTest, "StageShift.StageMemberCommand.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStageMemberCommandLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AStageManager* Manager = World->SpawnActor<AStageManager>();
	UStageMemberComponent* Member = NewObject<UStageMemberComponent>(Manager);
	Manager->AddInstanceComponent(Member);
	FStageExecutionContext Context;
	Context.Manager = Manager;
	Context.MemberComponent = Member;

	const auto MakeCommand = [&]()
	{
		auto* Command = NewObject<UStageMemberCommandLifecycleTestCommand>(Member);
		Command->ResetRuntime(Context, nullptr, 0.0f, 10.0f);
		Command->OnFinished.AddDynamic(Command, &UStageMemberCommandLifecycleTestCommand::RecordFinished);
		return Command;
	};
	{
		auto* Command = MakeCommand();
		Command->bFinishDuringBegin = true;
		Command->Start(Context);
		Command->Start(Context);
		Command->Complete(EStageCommandResult::Failed);
		TestEqual(TEXT("Synchronous success is preserved"), Command->GetResult(), EStageCommandResult::Succeeded);
		TestEqual(TEXT("Start only once"), Command->BeginCount, 1);
		TestEqual(TEXT("Notification only once"), Command->NotificationCount, 1);
		TestEqual(TEXT("Final state precedes notification"), Command->StateDuringNotification, EStageExecutionState::Finished);
	}
	{
		auto* Command = MakeCommand();
		Command->Start(FStageExecutionContext());
		TestEqual(TEXT("Invalid context fails"), Command->GetResult(), EStageCommandResult::Failed);
		TestEqual(TEXT("Invalid context never begins work"), Command->BeginCount, 0);
		TestFalse(TEXT("A failed task has no successful Main"), Command->HasSuccessfulMainAction());
	}
	{
		auto* Command = MakeCommand();
		Command->bDeferStop = true;
		Command->Start(Context);
		Command->RequestStop(EStageActionStopReason::Finished);
		Command->RequestStop(EStageActionStopReason::Finished);
		TestEqual(TEXT("One stop request"), Command->StopCount, 1);
		TestEqual(TEXT("Wait for stop acknowledgement"), Command->NotificationCount, 0);
		Command->Complete(EStageCommandResult::Succeeded);
		TestEqual(TEXT("Final cancellation overrides late success"), Command->GetResult(), EStageCommandResult::Cancelled);
	}
	{
		auto* Command = MakeCommand();
		Command->Complete(EStageCommandResult::Blocked);
		Command->Start(Context);
		TestEqual(TEXT("Admission can finish before Start"), Command->GetResult(), EStageCommandResult::Blocked);
		TestEqual(TEXT("A blocked task never starts"), Command->BeginCount, 0);
	}
	{
		auto* Command = MakeCommand();
		World->TimeSeconds = 100.0;
		Command->Start(Context);
		Command->NotifyMainActionStarted();
		World->TimeSeconds = 104.0;
		TestEqual(TEXT("Four seconds of normal Main"), Command->GetRemainingTime(), 6.0f);

		auto* InterruptedAction = NewObject<UStageActionLifecycleTestAction>(Member);
		InterruptedAction->Start(Context);
		InterruptedAction->Complete(EStageActionResult::Interrupted);
		Command->ApplyMainActionResult(InterruptedAction, EStageActionResult::Interrupted);
		World->TimeSeconds = 107.0; // Three seconds of Recovery; no Main is running.
		TestEqual(TEXT("Recovery does not consume speech"), Command->GetRemainingTime(), 6.0f);
		TestEqual(TEXT("Interrupted Action does not finish Command"), Command->GetState(), EStageExecutionState::Running);
		TestFalse(TEXT("Interrupted Main is not success"), Command->IsTaskSatisfied(Context));
		TestEqual(TEXT("Subclass receives interruption"), Command->LastActionResult, EStageActionResult::Interrupted);

		Command->SelectShortVersion();
		TestEqual(TEXT("Short version starts with a full three seconds"), Command->GetRemainingTime(), 3.0f);
		TestEqual(TEXT("Normal estimate stays unchanged"), Command->GetNeededTime(), 10.0f);
		TestEqual(TEXT("Planned end stays unchanged"), Command->GetPlannedEndTime(), 10.0f);
		Command->NotifyMainActionStarted();
		World->TimeSeconds = 110.0;
		auto* SuccessfulAction = NewObject<UStageActionLifecycleTestAction>(Member);
		SuccessfulAction->bFinishSynchronously = true;
		SuccessfulAction->Start(Context);
		Command->ApplyMainActionResult(SuccessfulAction, EStageActionResult::Succeeded);
		TestTrue(TEXT("Successful Main satisfies default task"), Command->IsTaskSatisfied(Context));
		Command->Complete(EStageCommandResult::Succeeded);
		TestEqual(TEXT("One final Command notification"), Command->NotificationCount, 1);
		TestEqual(TEXT("Finished task has no remaining time"), Command->GetRemainingTime(), 0.0f);

		Command->ScoreValue = 123;
		Command->PlaybackPosition = 4.0f;
		auto* Copy = DuplicateObject<UStageMemberCommandLifecycleTestCommand>(Command, Member);
		Copy->ResetRuntime(Context, nullptr, 20.0f, 30.0f);
		TestEqual(TEXT("Working copy keeps authored score"), Copy->ScoreValue, 123);
		TestEqual(TEXT("Working copy resets state"), Copy->GetState(), EStageExecutionState::NotStarted);
		TestEqual(TEXT("Working copy resets result"), Copy->GetResult(), EStageCommandResult::None);
		TestFalse(TEXT("Working copy resets successful Main"), Copy->HasSuccessfulMainAction());
		TestFalse(TEXT("Working copy resets short version"), Copy->IsUsingShortVersion());
		TestEqual(TEXT("Working copy resets subclass progress"), Copy->PlaybackPosition, 0.0f);
		TestFalse(TEXT("Working copy clears copied subscriptions"), Copy->OnFinished.IsBound());
		TestEqual(TEXT("Working copy has its own root"), Copy->GetRootCommand(), static_cast<UStageMemberCommand*>(Copy));
		TestEqual(TEXT("Working copy receives its new plan"), Copy->GetPlannedStartTime(), 20.0f);
		TestTrue(TEXT("Original remains finished"), Command->GetState() == EStageExecutionState::Finished);
	}

	World->DestroyWorld(false);
	return true;
}

#endif
