#include "Tests/StageMemberExecutionTestTypes.h"

#include "Components/Stage/StageMemberComponent.h"
#include "StatusesComponent.h"
#include "NativeGameplayTags.h"
#include "GameFramework/Actor.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StageExecutionTest_Fallen, "Stage.Test.Fallen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StageExecutionTest_Ragdoll, "Stage.Test.Fallen.Ragdoll");

void UStageMemberExecutionTestAction::Execute_Implementation()
{
	if (bRemoveStatusOnStart)
	{
		if (auto* Statuses = Context.MemberComponent->GetOwner()->FindComponentByClass<UStatusesComponent>())
			Statuses->RemoveStatuses_Implementation(TAG_StageExecutionTest_Ragdoll.GetTag().GetSingleTagContainer());
	}
	if (bAutoFinish) Complete(EStageActionResult::Succeeded);
}

void UStageMemberExecutionTestAction::StopNative_Implementation(EStageActionStopMode Mode)
{
	++StopCount;
	if (!bDeferStop) Super::StopNative_Implementation(Mode);
}

UStageAction* UStageMemberExecutionTestCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	++CreateCount;
	LastMainAction = NewObject<UStageMemberExecutionTestAction>(InContext.MemberComponent.Get());
	LastMainAction->bAutoFinish = bAutoFinishMain;
	return LastMainAction;
}

bool UStageMemberExecutionTestCommand::IsTaskSatisfied_Implementation(const FStageExecutionContext& InContext) const
{
	return bTaskAlreadySatisfied || Super::IsTaskSatisfied_Implementation(InContext);
}

void UStageMemberExecutionTestCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult Result)
{
	LastMainResult = Result;
}

void UStageMemberExecutionTestCommand::HandleCommandReported(UStageMemberCommand* Command, EStageCommandResult Result)
{
	if (Command == this && bEndMemberOnReport) Context.MemberComponent->EndStageMember();
}

void UStageMemberExecutionTestCommand::HandleOwnFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	bMemberFinishedInsideCallback = Context.MemberComponent->IsMemberFinished();
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Managers/Stage/Data/StageRecoveryRulesDataAsset.h"
#include "Managers/Stage/StageManager.h"
#include "Misc/AutomationTest.h"
#include "UObject/GarbageCollection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStageMemberExecutionTest, "StageShift.StageMemberComponent.Execution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FExecutionTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AStageManager* Manager = World->SpawnActor<AStageManager>();
		UStatusesComponent* Statuses = NewObject<UStatusesComponent>(Manager);
		UStageMemberComponent* Member = NewObject<UStageMemberComponent>(Manager);
		FStageExecutionContext Context;

		FExecutionTestWorld()
		{
			Manager->AddInstanceComponent(Statuses);
			Manager->AddOwnedComponent(Statuses);
			Manager->AddInstanceComponent(Member);
			Manager->AddOwnedComponent(Member);
			Statuses->RegisterComponent();
			Member->RegisterComponent();
			Context.Manager = Manager;
			Context.MemberComponent = Member;
		}
		~FExecutionTestWorld() { World->DestroyWorld(false); }

		UStageMemberExecutionTestCommand* MakeCommand(bool bDenyFallen = false)
		{
			auto* Command = NewObject<UStageMemberExecutionTestCommand>(Member);
			Command->ResetRuntime(Context, nullptr, 0.0f, 10.0f);
			if (bDenyFallen) Command->ExecutionRequirements.DenyTags.AddTag(TAG_StageExecutionTest_Fallen);
			return Command;
		}
		void Fall() { Statuses->AddConstantStatuses_Implementation(TAG_StageExecutionTest_Ragdoll.GetTag().GetSingleTagContainer()); }
		void Tick() { Member->TickComponent(0.0f, LEVELTICK_All, nullptr); }
		void SetRecovery(bool bRemoveStatusOnStart)
		{
			Member->RecoveryRulesAsset = NewObject<UStageRecoveryRulesDataAsset>(Member);
			Member->RecoveryRulesAsset->RecoveryWaitTimeout = 0.5f;
			auto* Template = NewObject<UStageMemberExecutionTestAction>(Member->RecoveryRulesAsset);
			Template->bRemoveStatusOnStart = bRemoveStatusOnStart;
			FStageActionRule Rule;
			Rule.Action = Template;
			Rule.ExecRequirements.RequiredTags.AddTag(TAG_StageExecutionTest_Fallen);
			Member->RecoveryRulesAsset->Rules.Add(Rule);
		}
	};
}

bool FStageMemberExecutionTest::RunTest(const FString& Parameters)
{
	{
		FExecutionTestWorld Env;
		TestTrue(TEXT("Bind to existing statuses"), Env.Member->BindToStage(Env.Manager));
		auto* Command = Env.MakeCommand();
		Command->bAutoFinishMain = true;
		Command->bEndMemberOnReport = true;
		Env.Member->OnCommandFinished.AddDynamic(Command, &UStageMemberExecutionTestCommand::HandleCommandReported);
		Command->OnFinished.AddDynamic(Command, &UStageMemberExecutionTestCommand::HandleOwnFinished);
		Command->Start(Env.Context);
		TestEqual(TEXT("Synchronous Main completes task"), Command->GetResult(), EStageCommandResult::Succeeded);
		TestFalse(TEXT("Member waits for all completion listeners"), Command->bMemberFinishedInsideCallback);
		TestTrue(TEXT("Member finishes after the callbacks return"), Env.Member->IsMemberFinished());
		TestTrue(TEXT("A new stage can bind after cleanup"), Env.Member->BindToStage(Env.Manager));
		Env.Member->EndStageMember();
	}
	{
		FExecutionTestWorld Env;
		Env.Member->BindToStage(Env.Manager);
		auto* Command = Env.MakeCommand();
		Command->bTaskAlreadySatisfied = true;
		Command->Start(Env.Context);
		TestEqual(TEXT("Already achieved task succeeds"), Command->GetResult(), EStageCommandResult::Succeeded);
		TestEqual(TEXT("Already achieved task does not create an Action"), Command->CreateCount, 0);
		TestFalse(TEXT("Already achieved task has no successful own Main"), Command->HasSuccessfulMainAction());
		Env.Member->EndStageMember();
	}
	{
		FExecutionTestWorld Env;
		Env.SetRecovery(true);
		Env.Member->BindToStage(Env.Manager);
		auto* First = Env.MakeCommand(true);
		auto* Second = Env.MakeCommand(true);
		First->Start(Env.Context);
		Second->Start(Env.Context);
		Env.Fall(); // This plugin entry point does not broadcast; Tick must still notice it.
		Env.Tick();
		Env.Tick();
		auto* Recovery = Cast<UStageMemberExecutionTestAction>(Env.Member->GetRecoveryAction());
		TestNotNull(TEXT("A shared recovery starts"), Recovery);
		TestEqual(TEXT("First Main was interrupted, Command still runs"), First->LastMainResult, EStageActionResult::Interrupted);
		TestEqual(TEXT("First Command remains running"), First->GetState(), EStageExecutionState::Running);
		TestEqual(TEXT("Second Main was interrupted"), Second->LastMainResult, EStageActionResult::Interrupted);
		TestFalse(TEXT("Recovery may remove the status before it finishes"), Env.Statuses->GetStatuses().HasTag(TAG_StageExecutionTest_Fallen));
		Env.Tick();
		TestEqual(TEXT("Main does not resume before Recovery confirmation"), Second->CreateCount, 1);
		First->RequestStop(EStageActionStopReason::Finished);
		TestEqual(TEXT("One waiter can cancel"), First->GetResult(), EStageCommandResult::Cancelled);
		if (Recovery)
		{
			TestEqual(TEXT("Remaining waiter keeps shared recovery alive"), Recovery->StopCount, 0);
			Recovery->Complete(EStageActionResult::Succeeded);
		}
		TestEqual(TEXT("Live task creates a new Main after recovery"), Second->CreateCount, 2);
		TestEqual(TEXT("Cancelled task does not restart"), First->CreateCount, 1);
		if (Second->LastMainAction) Second->LastMainAction->Complete(EStageActionResult::Succeeded);
		TestEqual(TEXT("Recovered task succeeds"), Second->GetResult(), EStageCommandResult::Succeeded);
		Env.Member->EndStageMember();
		TestTrue(TEXT("Recovered stage participation cleans up"), Env.Member->IsMemberFinished());
	}
	{
		FExecutionTestWorld Env;
		Env.SetRecovery(false);
		Env.Member->BindToStage(Env.Manager);
		auto* Unrelated = Env.MakeCommand();
		Unrelated->Start(Env.Context);
		Env.Fall();
		auto* Waiting = Env.MakeCommand(true);
		Waiting->Start(Env.Context);
		auto* Recovery = Cast<UStageMemberExecutionTestAction>(Env.Member->GetRecoveryAction());
		if (TestNotNull(TEXT("Recovery created for blocked task"), Recovery)) Recovery->Complete(EStageActionResult::Failed);
		TestEqual(TEXT("Recovery failure fails its waiter"), Waiting->GetResult(), EStageCommandResult::Failed);
		TestEqual(TEXT("Recovery failure does not cancel unrelated Main"), Unrelated->GetState(), EStageExecutionState::Running);
		Env.Member->EndStageMember();
	}
	{
		FExecutionTestWorld Env;
		Env.Member->RecoveryRulesAsset = NewObject<UStageRecoveryRulesDataAsset>(Env.Member);
		Env.Member->RecoveryRulesAsset->RecoveryWaitTimeout = 0.5f;
		Env.Member->BindToStage(Env.Manager);
		Env.Fall();
		auto* Command = Env.MakeCommand(true);
		Command->Start(Env.Context);
		TestEqual(TEXT("Unavailable Recovery initially waits"), Command->GetState(), EStageExecutionState::Running);
		Env.World->TimeSeconds += 1.0;
		Env.Tick();
		TestEqual(TEXT("Unavailable Recovery wait is bounded"), Command->GetResult(), EStageCommandResult::Failed);
		Env.Member->EndStageMember();
	}
	{
		FExecutionTestWorld Env;
		Env.Member->BindToStage(Env.Manager);
		auto* Command = Env.MakeCommand();
		Command->Start(Env.Context);
		auto* Main = Command->LastMainAction.Get();
		Main->bDeferStop = true;
		Command->LastMainAction = nullptr;
		CollectGarbage(RF_NoFlags);
		TestTrue(TEXT("Member UPROPERTY retains the active Action"), IsValid(Main));
		Env.Member->EndStageMember();
		TestEqual(TEXT("Cancellation waits for native acknowledgement"), Command->GetState(), EStageExecutionState::Stopping);
		TestFalse(TEXT("Stage does not finish with an active stopping Action"), Env.Member->IsMemberFinished());
		Main->Complete(EStageActionResult::Succeeded);
		TestEqual(TEXT("Late Main success cannot undo final cancellation"), Command->GetResult(), EStageCommandResult::Cancelled);
		TestTrue(TEXT("Cleanup follows stop acknowledgement"), Env.Member->IsMemberFinished());
	}
	return true;
}

#endif
