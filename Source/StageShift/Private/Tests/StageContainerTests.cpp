#include "Tests/StageContainerTestCommand.h"

void UStageContainerTestCommand::BeginWork_Implementation()
{
	Super::BeginWork_Implementation();
	if (ParentToStopOnBegin)
	{
		ParentToStopOnBegin->RequestStop(EStageActionStopReason::Finished);
		bParentFinishedInsideBegin = ParentToStopOnBegin->GetState() == EStageExecutionState::Finished;
	}
}

void UStageContainerTestCommand::ObserveClosedTree(UStageMemberCommand* Command, EStageCommandResult Result)
{
	bWholeTreeClosedDuringNotification = true;
	TArray<UStageMemberCommand*> Pending;
	Pending.Add(ObservedRoot);
	while (!Pending.IsEmpty())
	{
		UStageMemberCommand* Node = Pending.Pop();
		if (!Node || Node->GetState() != EStageExecutionState::Finished)
		{
			bWholeTreeClosedDuringNotification = false;
			return;
		}
		for (UStageMemberCommand* Child : Node->GetChildCommands()) Pending.Add(Child);
	}
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "Managers/Stage/ParallelStageMemberCommand.h"
#include "Managers/Stage/SequenceStageMemberCommand.h"
#include "Managers/Stage/StageManager.h"
#include "Misc/AutomationTest.h"
#include "StatusesComponent.h"
#include "Tests/StageMemberExecutionTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStageContainerTest, "StageShift.StageCommands.Containers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FContainerTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AStageManager* Manager = World->SpawnActor<AStageManager>();
		UStatusesComponent* Statuses = NewObject<UStatusesComponent>(Manager);
		UStageMemberComponent* Member = NewObject<UStageMemberComponent>(Manager);
		FStageExecutionContext Context;

		FContainerTestWorld()
		{
			Manager->AddInstanceComponent(Statuses);
			Manager->AddOwnedComponent(Statuses);
			Manager->AddInstanceComponent(Member);
			Manager->AddOwnedComponent(Member);
			Statuses->RegisterComponent();
			Member->RegisterComponent();
			Member->BindToStage(Manager);
			Context.Manager = Manager;
			Context.MemberComponent = Member;
		}
		~FContainerTestWorld()
		{
			Member->EndStageMember();
			World->DestroyWorld(false);
		}
		UStageContainerTestCommand* Leaf(UObject* Outer, float Duration = 2.0f)
		{
			auto* Child = NewObject<UStageContainerTestCommand>(Outer);
			Child->EstimatedDuration = Duration;
			return Child;
		}
		void Prepare(UStageMemberCommand* Root, float Start = 0.0f)
		{
			Root->ResetRuntime(Context, nullptr, Start, Start + Root->GetNeededTime());
		}
	};
}

bool FStageContainerTest::RunTest(const FString& Parameters)
{
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<USequenceStageMemberCommand>(Env.Member);
		auto* First = Env.Leaf(Root, 2.0f);
		auto* Second = Env.Leaf(Root, 3.0f);
		Root->Children = {First, Second};
		Env.Prepare(Root, 5.0f);
		TestEqual(TEXT("Sequence normal duration is a sum"), Root->GetNeededTime(), 5.0f);
		TestEqual(TEXT("Second child has an offset in the normal plan"), Second->GetPlannedStartTime(), 7.0f);
		Env.Member->StartPreparedCommand(Root);
		TestEqual(TEXT("Sequence starts first child"), First->BeginCount, 1);
		TestEqual(TEXT("Sequence waits before starting second child"), Second->BeginCount, 0);
		First->Complete(EStageCommandResult::Failed);
		TestEqual(TEXT("Sequence continues after child failure"), Second->BeginCount, 1);
		TestEqual(TEXT("Sequence still waits for second result"), Root->GetState(), EStageExecutionState::Running);
		Second->Complete(EStageCommandResult::Succeeded);
		TestEqual(TEXT("Sequence remembers child failure in final result"), Root->GetResult(), EStageCommandResult::Failed);
		TestEqual(TEXT("Finished Sequence has no remaining duration"), Root->GetRemainingTime(), 0.0f);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<UParallelStageMemberCommand>(Env.Member);
		auto* First = Env.Leaf(Root, 2.0f);
		auto* Second = Env.Leaf(Root, 5.0f);
		Root->Children = {First, Second};
		Env.Prepare(Root, 7.0f);
		TestEqual(TEXT("Parallel normal duration is a maximum"), Root->GetNeededTime(), 5.0f);
		TestEqual(TEXT("Parallel children share their planned start"), Second->GetPlannedStartTime(), First->GetPlannedStartTime());
		Env.Member->StartPreparedCommand(Root);
		TestEqual(TEXT("Parallel starts both children"), First->BeginCount + Second->BeginCount, 2);
		First->Complete(EStageCommandResult::Failed);
		TestEqual(TEXT("Parallel leaves another child running after a failure"), Second->GetState(), EStageExecutionState::Running);
		TestEqual(TEXT("Parallel does not cancel the independent child"), Second->StopCount, 0);
		TestEqual(TEXT("Parallel waits for every child result"), Root->GetState(), EStageExecutionState::Running);
		Second->Complete(EStageCommandResult::Succeeded);
		TestEqual(TEXT("Parallel remembers the failed child"), Root->GetResult(), EStageCommandResult::Failed);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<USequenceStageMemberCommand>(Env.Member);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			auto* Child = Env.Leaf(Root);
			Child->bFinishDuringBegin = true;
			Root->Children.Add(Child);
		}
		Env.Prepare(Root);
		Env.Member->StartPreparedCommand(Root);
		TestEqual(TEXT("Synchronous children advance through OnFinished"), Root->GetResult(), EStageCommandResult::Succeeded);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<UParallelStageMemberCommand>(Env.Member);
		auto* First = Env.Leaf(Root);
		auto* Second = Env.Leaf(Root);
		Root->Children = {First, Second};
		First->ParentToStopOnBegin = Root;
		Env.Prepare(Root);
		Env.Member->StartPreparedCommand(Root);
		TestFalse(TEXT("Parallel does not finish inside its issuing loop"), First->bParentFinishedInsideBegin);
		TestEqual(TEXT("Cancellation during issuing blocks later starts"), Second->BeginCount, 0);
		TestEqual(TEXT("Cancelled pending child receives a result"), Second->GetResult(), EStageCommandResult::Cancelled);
		TestEqual(TEXT("Issuing cancellation finishes the container"), Root->GetResult(), EStageCommandResult::Cancelled);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<USequenceStageMemberCommand>(Env.Member);
		auto* First = Env.Leaf(Root);
		auto* Second = Env.Leaf(Root);
		Root->Children = {First, Second};
		First->bDeferStop = true;
		Env.Prepare(Root);
		Env.Member->StartPreparedCommand(Root);
		Env.Member->EndStageMember();
		TestEqual(TEXT("Sequence awaits the active child's stop acknowledgement"), Root->GetState(), EStageExecutionState::Stopping);
		TestEqual(TEXT("Future child is cancelled without starting"), Second->BeginCount, 0);
		TestEqual(TEXT("Future child has a terminal cancellation"), Second->GetResult(), EStageCommandResult::Cancelled);
		TestFalse(TEXT("Member cannot finish before the container"), Env.Member->IsMemberFinished());
		First->Complete(EStageCommandResult::Succeeded);
		TestEqual(TEXT("Sequence cancellation overrides late success"), Root->GetResult(), EStageCommandResult::Cancelled);
		TestTrue(TEXT("Member finishes after the full tree"), Env.Member->IsMemberFinished());
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<UParallelStageMemberCommand>(Env.Member);
		auto* First = Env.Leaf(Root);
		auto* Second = Env.Leaf(Root);
		Root->Children = {First, Second};
		Second->bDeferStop = true;
		Env.Prepare(Root);
		Env.Member->StartPreparedCommand(Root);
		First->Complete(EStageCommandResult::Succeeded);
		Root->RequestStop(EStageActionStopReason::Finished);
		TestEqual(TEXT("Parallel waits for stopping branches"), Root->GetState(), EStageExecutionState::Stopping);
		Second->Complete(EStageCommandResult::Succeeded);
		TestEqual(TEXT("Stopped Parallel is Cancelled"), Root->GetResult(), EStageCommandResult::Cancelled);
		TestEqual(TEXT("Earlier successful child stays successful"), First->GetResult(), EStageCommandResult::Succeeded);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<USequenceStageMemberCommand>(Env.Member);
		auto* First = Env.Leaf(Root);
		auto* Nested = NewObject<UParallelStageMemberCommand>(Root);
		auto* Second = Env.Leaf(Nested);
		auto* Third = Env.Leaf(Nested);
		Nested->Children = {Second, Third};
		Root->Children = {First, Nested};
		Root->CatchUpPolicy = EStageCatchUpPolicy::SkipBeforeStart;
		First->ObservedRoot = Root;
		Env.Prepare(Root);
		First->OnFinished.AddDynamic(First, &UStageContainerTestCommand::ObserveClosedTree);
		Env.Member->CompleteBeforeStart(Root, EStageCommandResult::Skipped);
		TestTrue(TEXT("Entire tree is closed before the first skip notification"), First->bWholeTreeClosedDuringNotification);
		TestEqual(TEXT("Skipped root has a terminal result"), Root->GetResult(), EStageCommandResult::Skipped);
		TestEqual(TEXT("Nested child is skipped even with Keep policy"), Third->GetResult(), EStageCommandResult::Skipped);
		TestEqual(TEXT("Skipped leaves never begin"), First->BeginCount + Second->BeginCount + Third->BeginCount, 0);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<UParallelStageMemberCommand>(Env.Member);
		auto* First = Env.Leaf(Root);
		auto* Second = Env.Leaf(Root);
		Root->Children = {First, Second};
		Second->bFinishDuringBegin = true;
		Env.Prepare(Root);
		Env.Member->CompleteBeforeStart(First, EStageCommandResult::Skipped);
		Env.Member->StartPreparedCommand(Root);
		TestEqual(TEXT("Success plus allowed skip is success"), Root->GetResult(), EStageCommandResult::Succeeded);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<UParallelStageMemberCommand>(Env.Member);
		auto* First = Env.Leaf(Root);
		auto* Second = Env.Leaf(Root);
		Root->Children = {First, Second};
		Env.Prepare(Root);
		Env.Member->CompleteBeforeStart(First, EStageCommandResult::Skipped);
		Env.Member->CompleteBeforeStart(Second, EStageCommandResult::Skipped);
		Env.Member->StartPreparedCommand(Root);
		TestEqual(TEXT("All skipped children produce Skipped"), Root->GetResult(), EStageCommandResult::Skipped);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<USequenceStageMemberCommand>(Env.Member);
		auto* Nested = NewObject<UParallelStageMemberCommand>(Root);
		auto* First = Env.Leaf(Nested, 2.0f);
		auto* Second = Env.Leaf(Nested, 5.0f);
		auto* Tail = Env.Leaf(Root, 3.0f);
		Nested->Children = {First, Second};
		Root->Children = {Nested, Tail};
		Env.Prepare(Root, 11.0f);
		TestEqual(TEXT("Nested normal plan uses sum of maximum and tail"), Root->GetNeededTime(), 8.0f);
		TestEqual(TEXT("Tail starts after the normal parallel maximum"), Tail->GetPlannedStartTime(), 16.0f);
		Root->FreezeFinaleChoice(Env.Context);
		TestEqual(TEXT("Finale choice reaches every leaf"), First->FreezeCount + Second->FreezeCount + Tail->FreezeCount, 3);
		auto* Copy = DuplicateObject<USequenceStageMemberCommand>(Root, Env.Member);
		Copy->ResetRuntime(Env.Context, nullptr, 20.0f, 28.0f);
		auto* CopyNested = Cast<UParallelStageMemberCommand>(Copy->Children[0]);
		if (TestNotNull(TEXT("Nested copy preserves container class"), CopyNested))
		{
			TestTrue(TEXT("Working copy duplicates children"), CopyNested != Nested && CopyNested->Children[0] != First);
			TestEqual(TEXT("Working child's Outer is its copied container"), CopyNested->Children[0]->GetOuter(), static_cast<UObject*>(CopyNested));
			TestEqual(TEXT("All descendants share the copied root"), CopyNested->Children[0]->GetRootCommand(), static_cast<UStageMemberCommand*>(Copy));
			TestEqual(TEXT("Copied tail receives a shifted plan"), Copy->Children[1]->GetPlannedStartTime(), 25.0f);
		}
		TestEqual(TEXT("Copy preparation does not change the source plan"), Tail->GetPlannedStartTime(), 16.0f);
	}
	{
		FContainerTestWorld Env;
		auto* Empty = NewObject<USequenceStageMemberCommand>(Env.Member);
		Env.Prepare(Empty);
		Env.Member->StartPreparedCommand(Empty);
		TestEqual(TEXT("Empty container fails without hanging"), Empty->GetResult(), EStageCommandResult::Failed);
		auto* Root = NewObject<UParallelStageMemberCommand>(Env.Member);
		auto* Shared = Env.Leaf(Root);
		Root->Children = {Shared, Shared};
		Env.Prepare(Root);
		Env.Member->StartPreparedCommand(Root);
		TestEqual(TEXT("Duplicate child is rejected"), Root->GetResult(), EStageCommandResult::Failed);
		TestEqual(TEXT("Invalid tree starts no child"), Shared->BeginCount, 0);
	}
	{
		FContainerTestWorld Env;
		auto* Root = NewObject<UParallelStageMemberCommand>(Env.Member);
		auto* First = NewObject<UStageMemberExecutionTestCommand>(Root);
		auto* Second = NewObject<UStageMemberExecutionTestCommand>(Root);
		Root->Children = {First, Second};
		Env.Prepare(Root);
		Env.Member->StartPreparedCommand(Root);
		TestEqual(TEXT("Parallel registers and starts independent Main Actions"), First->CreateCount + Second->CreateCount, 2);
		if (First->LastMainAction) First->LastMainAction->Complete(EStageActionResult::Succeeded);
		TestEqual(TEXT("One finished Main does not finish the Parallel"), Root->GetState(), EStageExecutionState::Running);
		if (Second->LastMainAction) Second->LastMainAction->Complete(EStageActionResult::Succeeded);
		TestEqual(TEXT("Parallel finishes after both actual Actions"), Root->GetResult(), EStageCommandResult::Succeeded);
	}
	return true;
}

#endif
