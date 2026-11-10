#include "Tests/StageManagerTestTypes.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "Managers/Stage/Data/StageDataAsset.h"
#include "Managers/Stage/Data/StageRecoveryRulesDataAsset.h"
#include "Managers/Stage/StageManager.h"
#include "Managers/Stage/SequenceStageMemberCommand.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "StatusesComponent.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ManagerTest_A, "Stage.Test.Member.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ManagerTest_B, "Stage.Test.Member.B");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ManagerTest_Stage, "Stage.Test.Scene");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ManagerTest_Fallen, "Stage.Test.Manager.Fallen");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ManagerTest_Event, "Stage.Test.Manager.Laugh");

void UStageManagerTestRecorder::StageCompleted(FGameplayTag StageId)
{
	++Completed;
	FinalScore = Manager->GetScore();
	bIdleDuringCompletion = Manager->GetState() == EStageState::Idle && Manager->GetMember(TAG_ManagerTest_A) == nullptr;
	if (NextStage)
	{
		UStageDataAsset* Next = NextStage;
		NextStage = nullptr;
		FText Error;
		bNextAccepted = Manager->StartStage(Next, Actors, Error);
	}
}

void UStageManagerTestRecorder::StageFailed(FGameplayTag StageId, FText Reason) { ++Failed; }
void UStageManagerTestRecorder::AddAudienceScore(FStageEvent Event) { Manager->AddScore(7); }

void UStageManagerTestAction::Execute_Implementation()
{
	if (bRecovery)
		Context.MemberComponent->GetOwner()->FindComponentByClass<UStatusesComponent>()->RemoveStatuses_Implementation(
			TAG_ManagerTest_Fallen.GetTag().GetSingleTagContainer());
	if (bAutoFinish) Complete(EStageActionResult::Succeeded);
}

EStageActionStopMode UStageManagerTestAction::GetStopMode_Implementation(EStageActionStopReason Reason) const
{
	return bFinishFragment && Reason == EStageActionStopReason::Finished ? EStageActionStopMode::FinishAction : EStageActionStopMode::StopAction;
}

void UStageManagerTestCommand::BeginWork_Implementation()
{
	Recorder->Commands.Add(this);
	Super::BeginWork_Implementation();
}

bool UStageManagerTestCommand::IsTaskSatisfied_Implementation(const FStageExecutionContext& InContext) const
{
	return Recorder->Satisfied.Contains(Label) || Super::IsTaskSatisfied_Implementation(InContext);
}

UStageAction* UStageManagerTestCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	++ActionCount;
	LastAction = NewObject<UStageManagerTestAction>(InContext.MemberComponent.Get());
	LastAction->bAutoFinish = bAutoFinish;
	LastAction->bFinishFragment = bFinishFragment;
	return LastAction;
}

void UStageManagerTestCommand::OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult Result)
{
	if (Result == EStageActionResult::Succeeded) Recorder->Satisfied.Add(Label);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStageManagerTest, "StageShift.StageManager.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FManagerTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AStageManager* Manager = World->SpawnActor<AStageManager>();
		UStageManagerTestRecorder* Recorder = NewObject<UStageManagerTestRecorder>(Manager);
		AActor* A = World->SpawnActor<AActor>();
		AActor* B = World->SpawnActor<AActor>();
		UStageMemberComponent* MemberA = AddComponents(A);
		UStageMemberComponent* MemberB = AddComponents(B);
		UStageDataAsset* Data = NewObject<UStageDataAsset>(Manager);

		static UStageMemberComponent* AddComponents(AActor* Actor)
		{
			auto* Statuses = NewObject<UStatusesComponent>(Actor);
			auto* Member = NewObject<UStageMemberComponent>(Actor);
			Actor->AddInstanceComponent(Statuses);
			Actor->AddOwnedComponent(Statuses);
			Actor->AddInstanceComponent(Member);
			Actor->AddOwnedComponent(Member);
			Statuses->RegisterComponent();
			Member->RegisterComponent();
			return Member;
		}
		FManagerTestWorld()
		{
			Recorder->Manager = Manager;
			Recorder->Actors = {{TAG_ManagerTest_A, A}, {TAG_ManagerTest_B, B}};
			Manager->OnStageCompleted.AddDynamic(Recorder, &UStageManagerTestRecorder::StageCompleted);
			Manager->OnStageFailed.AddDynamic(Recorder, &UStageManagerTestRecorder::StageFailed);
			Data->StageId = TAG_ManagerTest_Stage;
			Data->MainDuration = 180.0f;
			Data->AdditiveDuration = 0.0f;
			Data->StageMembers = {{TAG_ManagerTest_A, A}, {TAG_ManagerTest_B, B}};
		}
		~FManagerTestWorld()
		{
			MemberA->EndStageMember();
			MemberB->EndStageMember();
			World->DestroyWorld(false);
		}
		UStageManagerTestCommand* Command(FName Label, int32 Score = 10, bool bAuto = false)
		{
			auto* C = NewObject<UStageManagerTestCommand>(Data);
			C->Recorder = Recorder; C->Label = Label; C->ScoreValue = Score; C->bAutoFinish = bAuto;
			return C;
		}
		FStageMemberAssignment Assign(FGameplayTag Role, UStageMemberCommand* C) { FStageMemberAssignment R; R.MemberTag = Role; R.Command = C; return R; }
		void Step(std::initializer_list<FStageMemberAssignment> Items) { FStageStep S; for (const auto& I : Items) S.Commands.Add(I); Data->Steps.Add(S); }
		bool Start() { FText Error; return Manager->StartStage(Data, {}, Error); }
		void Pump(float Seconds = 0.0f)
		{
			World->TimeSeconds += Seconds;
			Manager->Tick(Seconds);
			MemberA->TickComponent(Seconds, LEVELTICK_All, nullptr);
			MemberB->TickComponent(Seconds, LEVELTICK_All, nullptr);
			Manager->Tick(0.0f);
		}
		UStageManagerTestCommand* Working(FName Label)
		{
			for (UStageManagerTestCommand* C : Recorder->Commands) if (C->Label == Label) return C;
			return nullptr;
		}
	};
}

bool FStageManagerTest::RunTest(const FString& Parameters)
{
	// The lightweight test world has no InitializeActorsForPlay; allow actor delegate callbacks.
	TGuardValue<bool> AllowActorCallbacks(GAllowActorScriptExecutionInEditor, true);
	{
		FManagerTestWorld E;
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("SpeakA")), E.Assign(TAG_ManagerTest_B, E.Command("SpeakB"))});
		E.Step({E.Assign(TAG_ManagerTest_B, E.Command("Die", 30, true))});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die", 500, true))};
		E.Manager->StarThresholds = {30, 50, 100};
		TestTrue(TEXT("Valid Stage starts from explicit soft members"), E.Start());
		E.Pump();
		TestNotNull(TEXT("Both Step assignments start"), E.Working("SpeakB"));
		E.Working("SpeakA")->LastAction->Complete(EStageActionResult::Succeeded);
		TestNull(TEXT("One finished role does not advance Step"), E.Working("Die"));
		E.Working("SpeakB")->LastAction->Complete(EStageActionResult::Succeeded);
		E.Pump(); E.Pump();
		TestEqual(TEXT("Early completion ends Stage"), E.Recorder->Completed, 1);
		TestEqual(TEXT("Finale does not repeat an achieved result or award"), E.Manager->GetScore(), 50);
		TestEqual(TEXT("Stars count reached thresholds"), E.Manager->GetStarCount(), 2);
		TestTrue(TEXT("Completion sees cleaned Idle"), E.Recorder->bIdleDuringCompletion);
		TestEqual(TEXT("Template remains NotStarted"), E.Data->Steps[0].Commands[0].Command->GetState(), EStageExecutionState::NotStarted);
	}
	{
		FManagerTestWorld E;
		auto* Speech = E.Command("LongSpeech", 100); Speech->bFinishFragment = true;
		E.Step({E.Assign(TAG_ManagerTest_A, Speech)});
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("NeverStart"))});
		E.Data->MainDuration = 3.0f;
		E.Data->Finale = {E.Assign(TAG_ManagerTest_A, E.Command("Die", 40, true))};
		TestTrue(TEXT("Deadline Stage accepted"), E.Start()); E.Pump();
		E.Pump(3.0f);
		TestEqual(TEXT("Deadline requests Finale"), E.Manager->GetState(), EStageState::Finalizing);
		TestNull(TEXT("Finale waits for allowed speech fragment"), E.Working("Die"));
		TestEqual(TEXT("No completion before fragment stops"), E.Recorder->Completed, 0);
		E.Working("LongSpeech")->LastAction->Complete(EStageActionResult::Succeeded);
		E.Pump(); E.Pump();
		TestEqual(TEXT("Cancelled speech is not rewarded"), E.Manager->GetScore(), 40);
		TestEqual(TEXT("Finale finishes Stage"), E.Recorder->Completed, 1);
		TestNull(TEXT("Future Step never issued"), E.Working("NeverStart"));
	}
	{
		FManagerTestWorld E;
		auto* Speech = E.Command("Speech", 25);
		Speech->ExecutionRequirements.DenyTags.AddTag(TAG_ManagerTest_Fallen);
		E.Step({E.Assign(TAG_ManagerTest_A, Speech)});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die", 10, true))};
		auto* Rules = NewObject<UStageRecoveryRulesDataAsset>(E.MemberA);
		auto* Recovery = NewObject<UStageManagerTestAction>(Rules); Recovery->bRecovery = true; Recovery->bAutoFinish = true;
		FStageActionRule Rule; Rule.Action = Recovery; Rule.ExecRequirements.RequiredTags.AddTag(TAG_ManagerTest_Fallen);
		Rules->Rules = {Rule}; Rules->RecoveryWaitTimeout = 1.0f; E.MemberA->RecoveryRulesAsset = Rules;
		TestTrue(TEXT("Recovery Stage accepted"), E.Start()); E.Pump();
		auto* Working = E.Working("Speech");
		E.A->FindComponentByClass<UStatusesComponent>()->AddConstantStatuses_Implementation(TAG_ManagerTest_Fallen.GetTag().GetSingleTagContainer());
		E.Pump(); E.Pump();
		TestEqual(TEXT("Recovered Command creates a new Main"), Working->ActionCount, 2);
		TestEqual(TEXT("Recovery keeps same Command"), E.Working("Speech"), Working);
		Working->LastAction->Complete(EStageActionResult::Succeeded); E.Pump(); E.Pump();
		TestEqual(TEXT("Recovery Stage completes"), E.Recorder->Completed, 1);
	}
	{
		FManagerTestWorld E;
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("Fail"))});
		auto* Blocked = E.Command("Blocked"); Blocked->RequiredSuccessStepMemberTag = TAG_ManagerTest_A;
		E.Step({E.Assign(TAG_ManagerTest_B, Blocked)});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die", 10, true))};
		TestTrue(TEXT("Dependency on preceding Step is accepted"), E.Start()); E.Pump();
		E.Working("Fail")->LastAction->Complete(EStageActionResult::Failed); E.Pump(); E.Pump();
		TestNull(TEXT("Failed dependency blocks Command before BeginWork"), E.Working("Blocked"));
		TestEqual(TEXT("Blocked dependency does not hang Stage"), E.Recorder->Completed, 1);
	}
	{
		FManagerTestWorld E;
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("Main"))});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die", 10, true))};
		FStageMemberReaction R; R.EventTag = TAG_ManagerTest_Event; R.MemberTag = TAG_ManagerTest_A;
		R.Command = E.Command("Reaction", 15); R.MaxActivationsPerStage = 1; E.Data->MemberReactions.Add(R);
		E.Manager->OnStageEvent.AddDynamic(E.Recorder, &UStageManagerTestRecorder::AddAudienceScore);
		TestTrue(TEXT("Reaction Stage accepted"), E.Start()); E.Pump();
		FStageEvent Event; Event.EventTag = TAG_ManagerTest_Event; Event.SubjectActor = E.A;
		E.Manager->ReceiveStageEvent(Event); E.Manager->ReceiveStageEvent(Event);
		TestNull(TEXT("Reaction queues behind active root"), E.Working("Reaction"));
		E.Working("Main")->LastAction->Complete(EStageActionResult::Succeeded);
		E.Pump(); E.Pump();
		// End of the last Step requests Finale, which cancels a still-playing reaction.
		TestEqual(TEXT("Audience events award independently"), E.Manager->GetScore(), 34);
		TestEqual(TEXT("Finale waits for all stopped work"), E.Recorder->Completed, 1);
	}
	{
		FManagerTestWorld E;
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("First", 10, true))});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die", 20, true))};
		auto* Next = NewObject<UStageDataAsset>(E.Manager); Next->StageId = TAG_ManagerTest_Stage;
		Next->MainDuration = 180.0f; Next->AdditiveDuration = 0;
		FStageStep S; S.Commands = {E.Assign(TAG_ManagerTest_A, E.Command("Next", 30, true))}; Next->Steps = {S};
		Next->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("NextDie", 40, true))};
		E.Recorder->NextStage = Next;
		TestTrue(TEXT("First Stage accepted"), E.Start());
		for (int32 Index = 0; Index < 6; ++Index) E.Pump();
		TestTrue(TEXT("Next Stage can start inside completion"), E.Recorder->bNextAccepted);
		TestEqual(TEXT("Two Stages finish"), E.Recorder->Completed, 2);
		TestEqual(TEXT("Score persists across Stage changes"), E.Manager->GetScore(), 100);
	}
	{
		FManagerTestWorld E;
		auto* C = E.Command("Invalid"); C->RequiredSuccessStepMemberTag = TAG_ManagerTest_B;
		E.Step({E.Assign(TAG_ManagerTest_A, C)});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die"))};
		TestFalse(TEXT("First-Step dependency rejected before binding"), E.Start());
		TestTrue(TEXT("Rejected Stage leaves Member free"), E.MemberA->IsMemberFinished());
		TestEqual(TEXT("Rejected start does not broadcast runtime failure"), E.Recorder->Failed, 0);
	}
	{
		FManagerTestWorld E;
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("MainA")), E.Assign(TAG_ManagerTest_B, E.Command("HoldB"))});
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("Next", 20, true))});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die", 10, true))};
		FStageMemberReaction R; R.EventTag = TAG_ManagerTest_Event; R.MemberTag = TAG_ManagerTest_A;
		R.Command = E.Command("Reaction", 15); R.MaxActivationsPerStage = 1; E.Data->MemberReactions.Add(R);
		TestTrue(TEXT("Scoring reaction Stage starts"), E.Start()); E.Pump();
		FStageEvent Event; Event.EventTag = TAG_ManagerTest_Event; Event.SubjectActor = E.A;
		E.Manager->ReceiveStageEvent(Event); E.Manager->ReceiveStageEvent(Event);
		E.Working("MainA")->LastAction->Complete(EStageActionResult::Succeeded); E.Pump();
		auto* Reaction = E.Working("Reaction");
		if (TestNotNull(TEXT("Queued reaction starts after its actor is free"), Reaction))
		{
			Reaction->LastAction->Complete(EStageActionResult::Succeeded);
			TestEqual(TEXT("Reaction awards its own ScoreValue"), E.Manager->GetScore(), 25);
		}
		E.Manager->ReceiveStageEvent(Event); E.Pump();
		TestEqual(TEXT("Activation limit prevents another reaction root"), E.Recorder->Commands.Num(), 3);
		E.Working("HoldB")->LastAction->Complete(EStageActionResult::Succeeded); E.Pump(); E.Pump();
		TestEqual(TEXT("Reaction and scenario awards are independent"), E.Manager->GetScore(), 65);
	}
	{
		FManagerTestWorld E;
		auto* Sequence = NewObject<USequenceStageMemberCommand>(E.Data);
		auto* First = E.Command("First"); First->Rename(nullptr, Sequence);
		auto* TooLate = E.Command("TooLate"); TooLate->Rename(nullptr, Sequence);
		Sequence->Children = {First, TooLate};
		E.Step({E.Assign(TAG_ManagerTest_A, Sequence)});
		E.Data->MainDuration = 3.0f;
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die", 20, true))};
		TestTrue(TEXT("Nested deadline Stage starts"), E.Start()); E.Pump();
		E.World->TimeSeconds += 3.0;
		E.Working("First")->LastAction->Complete(EStageActionResult::Succeeded);
		TestNull(TEXT("Next Sequence child cannot start after deadline before Manager Tick"), E.Working("TooLate"));
		E.Pump(); E.Pump();
		TestEqual(TEXT("Deadline inside Sequence still completes Finale"), E.Recorder->Completed, 1);
	}
	{
		FManagerTestWorld E;
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("Late", 100))});
		auto* Skippable = E.Command("Skip"); Skippable->CatchUpPolicy = EStageCatchUpPolicy::SkipBeforeStart;
		E.Step({E.Assign(TAG_ManagerTest_A, Skippable)});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die", 10, true))};
		E.Manager->ScoreMultipliers.Add(EStageLatenessLevel::Easy, 0.5f);
		TestTrue(TEXT("Lateness Stage starts"), E.Start()); E.Pump(5.0f);
		TestEqual(TEXT("Current lateness uses the scenario plan"), E.Manager->GetCurrentLatenessLevel(), EStageLatenessLevel::Easy);
		E.Working("Late")->LastAction->Complete(EStageActionResult::Succeeded); E.Pump(); E.Pump();
		TestNull(TEXT("Late SkipBeforeStart command never enters BeginWork"), E.Working("Skip"));
		TestEqual(TEXT("Own lateness scales scenario award; Finale stays unscaled"), E.Manager->GetScore(), 60);
	}
	{
		FManagerTestWorld E;
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("Main", 10, true))});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("FailedFinale"))};
		TestTrue(TEXT("Mandatory failure Stage starts"), E.Start()); E.Pump(); E.Pump();
		auto* Finale = E.Working("FailedFinale");
		if (TestNotNull(TEXT("Mandatory Finale actually started"), Finale)) Finale->LastAction->Complete(EStageActionResult::Failed);
		E.Pump(); E.Pump();
		TestEqual(TEXT("Failed mandatory Finale reports failure once"), E.Recorder->Failed, 1);
		TestEqual(TEXT("Failed Finale never reports completion"), E.Recorder->Completed, 0);
		TestEqual(TEXT("Failure leaves cleaned Idle"), E.Manager->GetState(), EStageState::Idle);
	}
	{
		FManagerTestWorld E;
		E.Step({E.Assign(TAG_ManagerTest_A, E.Command("LateLast"))});
		E.Data->Finale = {E.Assign(TAG_ManagerTest_B, E.Command("Die"))};
		TestTrue(TEXT("Final timing sample Stage starts"), E.Start()); E.Pump(5.0f);
		E.Working("LateLast")->LastAction->Complete(EStageActionResult::Succeeded); E.Pump();
		TestEqual(TEXT("Finale preserves the last finished leaf's lateness"), E.Manager->GetCurrentLatenessLevel(), EStageLatenessLevel::Easy);
		E.Pump(5.0f);
		TestEqual(TEXT("Finale does not increase lateness"), E.Manager->GetCurrentLatenessLevel(), EStageLatenessLevel::Easy);
		E.Working("Die")->LastAction->Complete(EStageActionResult::Succeeded); E.Pump();
	}
	return true;
}
#endif
