#include "Tests/StageActionLifecycleTestAction.h"

void UStageActionLifecycleTestAction::RecordFinished(UStageAction* Action, EStageActionResult InResult)
{
	++NotificationCount;
	StateDuringNotification = GetState();
	NotifiedResult = InResult;
}

void UStageActionLifecycleTestAction::Execute_Implementation()
{
	++ExecuteCount;
	if (bFinishSynchronously)
	{
		Finish(EStageActionResult::Succeeded);
	}
}

EStageActionStopMode UStageActionLifecycleTestAction::GetStopMode_Implementation(EStageActionStopReason Reason) const
{
	return TestStopMode;
}

void UStageActionLifecycleTestAction::StopNative_Implementation(EStageActionStopMode Mode)
{
	++StopCount;
	if (!bDeferStop)
	{
		Super::StopNative_Implementation(Mode);
	}
}

void UStageActionLifecycleTestAction::Cleanup_Implementation()
{
	++CleanupCount;
	// A late or nested completion must not replace the committed result.
	Finish(EStageActionResult::Failed);
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Managers/Stage/StageManager.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStageActionLifecycleTest, "StageShift.StageAction.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStageActionLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}
	AStageManager* Manager = World->SpawnActor<AStageManager>();
	UStageMemberComponent* Member = NewObject<UStageMemberComponent>(Manager);
	Manager->AddInstanceComponent(Member);
	FStageExecutionContext Context;
	Context.Manager = Manager;
	Context.MemberComponent = Member;

	const auto MakeAction = [Member]()
	{
		UStageActionLifecycleTestAction* Action = NewObject<UStageActionLifecycleTestAction>(Member);
		Action->OnFinished.AddDynamic(Action, &UStageActionLifecycleTestAction::RecordFinished);
		return Action;
	};

	{
		auto Action = MakeAction();
		Action->bFinishSynchronously = true;
		Action->Start(Context);
		TestEqual(TEXT("Synchronous result"), Action->GetResult(), EStageActionResult::Succeeded);
		TestEqual(TEXT("State is final before notification"), Action->StateDuringNotification, EStageExecutionState::Finished);
		TestEqual(TEXT("Cleanup once"), Action->CleanupCount, 1);
		Action->Complete(EStageActionResult::Failed);
		Action->RequestStop(EStageActionStopReason::Finished);
		Action->Start(Context);
		TestEqual(TEXT("Single execution"), Action->ExecuteCount, 1);
		TestEqual(TEXT("Single notification despite nested and late Finish"), Action->NotificationCount, 1);
		TestEqual(TEXT("Final result preserved"), Action->NotifiedResult, EStageActionResult::Succeeded);
	}
	{
		auto Action = MakeAction();
		Action->Start(FStageExecutionContext());
		TestEqual(TEXT("Invalid context fails"), Action->GetResult(), EStageActionResult::Failed);
		TestEqual(TEXT("Invalid context never executes"), Action->ExecuteCount, 0);
		TestEqual(TEXT("Invalid context still notifies"), Action->NotificationCount, 1);
	}
	{
		auto Action = MakeAction();
		Action->Start(Context);
		TestTrue(TEXT("Default CanContinue"), Action->CanContinue(Context));
		TestEqual(TEXT("World comes from member"), Action->GetWorld(), World);
		Action->RequestStop(EStageActionStopReason::Interrupted);
		TestEqual(TEXT("Temporary stop result"), Action->GetResult(), EStageActionResult::Interrupted);
	}
	{
		auto Action = MakeAction();
		Action->bDeferStop = true;
		Action->Start(Context);
		Action->RequestStop(EStageActionStopReason::Interrupted);
		Action->RequestStop(EStageActionStopReason::Interrupted);
		TestEqual(TEXT("Deferred stop is still stopping"), Action->GetState(), EStageExecutionState::Stopping);
		TestEqual(TEXT("No premature notification"), Action->NotificationCount, 0);
		TestEqual(TEXT("Repeated stop is idempotent"), Action->StopCount, 1);
		Action->RequestStop(EStageActionStopReason::Finished);
		Action->Complete(EStageActionResult::Succeeded);
		TestEqual(TEXT("Final cancellation overrides late success after StopAction"), Action->GetResult(), EStageActionResult::Cancelled);
	}
	{
		auto Action = MakeAction();
		Action->TestStopMode = EStageActionStopMode::FinishAction;
		Action->Start(Context);
		Action->RequestStop(EStageActionStopReason::Finished);
		TestEqual(TEXT("Fragment waits for real completion"), Action->GetState(), EStageExecutionState::Stopping);
		Action->Complete(EStageActionResult::Succeeded);
		TestEqual(TEXT("Allowed fragment may succeed"), Action->GetResult(), EStageActionResult::Succeeded);
	}

	World->DestroyWorld(false);
	return true;
}

#endif
