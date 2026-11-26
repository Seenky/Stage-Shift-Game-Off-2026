#include "Stage/Core/StageManager.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageRecoveryRulesDataAsset.h"
#include "Stage/Core/Data/StageCommandParameters.h"
#include "Stage/Core/StageMemberCommand.h"
#include "Stage/Core/StageCondition.h"
#include "StatusesComponent.h"
#include "StatusesInfo.h"
#include "UObject/Package.h"
#include "Stage/Cues/StageCueComponent.h"
#include "Stage/Environment/StageTurntable.h"

DEFINE_LOG_CATEGORY(LogStageShiftStage);

void AStageManager::AddDiagnostic(const FString& Message)
{
	const FString Step = State == EStageState::Finalizing ? TEXT("Finale")
	    : ActiveStage && ActiveStage->Steps.IsValidIndex(CurrentStepIndex) && !ActiveStage->Steps[CurrentStepIndex].StepId.IsNone()
	    ? ActiveStage->Steps[CurrentStepIndex].StepId.ToString()
	    : FString::FromInt(CurrentStepIndex);
	const FString Line = FString::Printf(
	    TEXT("%s / Run[%s] / Step[%s] / %s"), ActiveStage ? *ActiveStage->GetEffectiveStageKey().ToString() : TEXT("None"), *RunId.ToString(EGuidFormats::Digits), *Step, *Message);
	RecentEvents.Add(FString::Printf(TEXT("[%.1f] %s"), GetStageElapsedTime(), *Message));
	UE_LOG(LogStageShiftStage, Display, TEXT("%s"), *Line);
}

FString AStageManager::DescribeCommand(const UStageMemberCommand* Command) const
{
	const auto* Entry = FindRoot(Command);
	if (!Entry) return GetNameSafe(Command);
	FString Path;
	if (Entry->CommandType == EStageCommandType::Finale)
		Path = TEXT("Finale");
	else if (Entry->CommandType == EStageCommandType::Reaction && ActiveStage && ActiveStage->MemberReactions.IsValidIndex(Entry->SourceIndex))
		Path = TEXT("Reaction[") + ActiveStage->MemberReactions[Entry->SourceIndex].ReactionId.ToString() + TEXT("]");
	else if (ActiveStage && ActiveStage->Steps.IsValidIndex(Entry->SourceIndex))
		Path = TEXT("Step[") + ActiveStage->Steps[Entry->SourceIndex].StepId.ToString() + TEXT("]");
	Path += TEXT(" / Role[") + Entry->RoleId.ToString() + TEXT("]");
	TFunction<bool(const UStageMemberCommand*, FString&)> FindChild = [&](const UStageMemberCommand* Parent, FString& Suffix)
	{
		if (Parent == Command) return true;
		int32 Index = 0;
		for (const UStageMemberCommand* Child : Parent->GetChildCommands())
		{
			FString ChildSuffix;
			if (Child && FindChild(Child, ChildSuffix))
			{
				Suffix = FString::Printf(TEXT(" / Child[%d]"), Index) + ChildSuffix;
				return true;
			}
			++Index;
		}
		return false;
	};
	FString Suffix;
	FindChild(Entry->Command, Suffix);
	return Path + Suffix + TEXT(" / ") + Command->GetClass()->GetName();
}

AStageManager::AStageManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PlayerCues = CreateDefaultSubobject<UStageCueComponent>(TEXT("Player Cues"));
	ScoreMultipliers.Add(EStageLatenessLevel::OnTime, 1.0f);
}

bool AStageManager::IsStageNavigationReady() const
{
	const AStageTurntable* Platform = ResolveTurntable();
	return !Platform || Platform->IsNavigationReady();
}

bool AStageManager::ValidateStage(UStageDataAsset* Data, const TMap<FName, AActor*>& Actors, FText& Error) const
{
	TArray<FString> Errors;
	CollectValidationErrors(Data, Actors, Errors, false);
	Error = FText::FromString(FString::Join(Errors, TEXT("\n")));
	return Errors.IsEmpty();
}

bool AStageManager::StartStage(UStageDataAsset* Data, const TMap<FGameplayTag, AActor*>& Overrides, FText& Error)
{
	TMap<FName, AActor*> ByIds;
	for (const auto& Pair : Overrides)
		ByIds.Add(Pair.Key.GetTagName(), Pair.Value);
	return StartStageByIds(Data, ByIds, Error);
}

bool AStageManager::StartStageByIds(UStageDataAsset* Data, const TMap<FName, AActor*>& Overrides, FText& Error)
{
	return StartStageWithBindings(Data, Overrides, TargetBindings, Error, false);
}

bool AStageManager::StartStageWithBindings(
    UStageDataAsset* Data, const TMap<FName, AActor*>& Overrides, const TMap<FName, TObjectPtr<AActor>>& Targets, FText& Error, bool bStrictRecovery)
{
	Error = FText::GetEmpty();
	if (bManagerEndingPlay || IsActorBeingDestroyed() || State != EStageState::Idle || !IsValid(Data) || !GetWorld() ||
	    (IsProductionActive() && !bStartingProductionScene) || (!PendingSceneBriefing.IsEmpty() && !bConfirmingSceneBriefing))
	{
		Error = FText::FromString(TEXT("Manager must be Idle and StageData must be valid."));
		return false;
	}
	if (!IsStageNavigationReady())
	{
		Error = NSLOCTEXT("StageNavigation", "WaitingForStage", "Дождись остановки сцены и обновления навигации.");
		return false;
	}
	const TArray<FName> Roles = Data->GetRoleOptions();
	for (const auto& Pair : Overrides)
		if (!Roles.Contains(Pair.Key))
		{
			Error = FText::FromString(TEXT("Unknown override RoleId: ") + Pair.Key.ToString());
			return false;
		}
	const TMap<FName, AActor*> Resolved = ResolveBindings(Data, Overrides);
	TGuardValue<bool> ValidationFlag(bUseValidationTargets, true);
	TGuardValue<TMap<FName, TObjectPtr<AActor>>> ValidationBindings(ValidationTargets, Targets);
	TArray<FString> Errors;
	if (!CollectValidationErrors(Data, Resolved, Errors, bStrictRecovery))
	{
		Error = FText::FromString(FString::Join(Errors, TEXT("\n")));
		return false;
	}
	TArray<UStageMemberComponent*> PreparedMembers;
	for (const auto& Pair : Resolved)
	{
		auto* Member = Pair.Value->FindComponentByClass<UStageMemberComponent>();
		if (!Member->BindToStage(this))
		{
			for (auto* Prepared : PreparedMembers)
				Prepared->EndStageMember();
			Error = FText::FromString(TEXT("Could not bind Stage members."));
			return false;
		}
		PreparedMembers.Add(Member);
	}
	ActiveStage = Data;
	ActiveTargetBindings = Targets;
	Members.Reset();
	BoundMembers.Reset();
	for (const auto& Pair : Resolved)
		Members.Add(Pair.Key, Pair.Value);
	for (auto* Member : PreparedMembers)
	{
		BoundMembers.Add(Member);
		Member->OnMemberFinished.AddUniqueDynamic(this, &AStageManager::HandleMemberFinished);
	}
	RootExecutions.Reset();
	AccountedCommands.Reset();
	PreviousStepResults.Reset();
	CurrentStepResults.Reset();
	ReactionActivationCounts.Init(0, Data->MemberReactions.Num());
	LastReactionStartedTimes.Init(-DBL_MAX, Data->MemberReactions.Num());
	RunId = FGuid::NewGuid();
	FailureReason = FText::GetEmpty();
	StageStartedAt = GetWorld()->GetTimeSeconds();
	StageElapsedTime = CurrentLatenessSeconds = MaxLatenessSeconds = 0.0f;
	CurrentLatenessLevel = EStageLatenessLevel::OnTime;
	CurrentStepIndex = INDEX_NONE;
	bStoryComplete = false;
	PlannedStepStarts.Reset();
	PreparedSteps = Data->Steps;
	const auto RejectPreparation = [&]()
	{
		for (const auto& Member : BoundMembers)
			if (Member.IsValid()) Member->EndStageMember();
		CleanupStage();
		return false;
	};
	TArray<float> StepDurations;
	PlannedStepStarts.Init(-1.0f, PreparedSteps.Num());
	for (auto& Step : PreparedSteps)
	{
		float Duration = 0.0f;
		for (auto& Assignment : Step.Commands)
		{
			Assignment.Command = CreatePreparedCommand(Assignment, Error);
			if (!Assignment.Command) return RejectPreparation();
			Duration = FMath::Max(Duration, Assignment.Command->GetNeededTime());
		}
		StepDurations.Add(Duration);
	}
	if (Data->bUseStepLinks)
	{
		TArray<int32> Planned;
		const auto PlanRoute = [&](int32 Index, float Start)
		{
			while (PreparedSteps.IsValidIndex(Index) && PlannedStepStarts[Index] < 0.0f)
			{
				PlannedStepStarts[Index] = Start;
				Planned.Add(Index);
				Start += StepDurations[Index];
				if (!FMath::IsFinite(Start)) return false;
				Index = Data->FindStepIndex(PreparedSteps[Index].ThenStepId);
			}
			return true;
		};
		// Keep the planned start of a join on the main route. Fallbacks are detours.
		bool bValidPlan = PlanRoute(Data->FindStepIndex(Data->EntryStepId), 0.0f);
		for (int32 Cursor = 0; bValidPlan && Cursor < Planned.Num(); ++Cursor)
		{
			const int32 Index = Planned[Cursor];
			bValidPlan = PlanRoute(Data->FindStepIndex(PreparedSteps[Index].FailedStepId), PlannedStepStarts[Index] + StepDurations[Index]);
		}
		if (!bValidPlan)
		{
			Error = FText::FromString(TEXT("Stage plan duration overflow."));
			return RejectPreparation();
		}
	}
	else
	{
		float Start = 0.0f;
		for (int32 Index = 0; Index < PreparedSteps.Num(); ++Index)
		{
			PlannedStepStarts[Index] = Start;
			Start += StepDurations[Index];
			if (!FMath::IsFinite(Start))
			{
				Error = FText::FromString(TEXT("Stage plan duration overflow."));
				return RejectPreparation();
			}
		}
	}
	StageStartedAt = GetWorld()->GetTimeSeconds();
	State = EStageState::Running;
	CloseSceneBriefing();
	PlayerCues->StartStage(Data);
	bAdvanceRequested = true;
	AdvanceStage();
	return true;
}

AActor* AStageManager::GetMember(FGameplayTag Tag) const
{
	const auto* Actor = Members.Find(Tag.GetTagName());
	return Actor ? Actor->Get() : nullptr;
}

float AStageManager::GetStageElapsedTime() const
{
	return State != EStageState::Idle && GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - StageStartedAt) : StageElapsedTime;
}

void AStageManager::AddScore(int32 Amount)
{
	const int64 Previous = Score;
	Score = static_cast<int32>(FMath::Clamp<int64>(static_cast<int64>(Score) + Amount, MIN_int32, MAX_int32));
	if (Score != Previous) OnScoreChanged.Broadcast(static_cast<int32>(Score - Previous));
}

int32 AStageManager::GetStarCount() const
{
	int32 Count = 0;
	for (int32 Threshold : StarThresholds)
		if (Score >= Threshold) ++Count;
	return Count;
}

void AStageManager::RecalculateEstimatedScore()
{
	int64 Total = 0;
	const auto AddStageScore = [&Total](UStageDataAsset* Data)
	{
		if (!IsValid(Data)) return;
		Data->RecalculateEstimatedScore();
		Total += Data->EstimatedScore;
	};
	if (IsValid(ConfiguredProduction))
	{
		for (const FStageProductionScene& Scene : ConfiguredProduction->Scenes)
			AddStageScore(Scene.Scenario);
	}
	else
	{
		AddStageScore(ConfiguredStage);
	}
	EstimatedTotalScore = static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

UStageMemberCommand* AStageManager::CreatePreparedCommand(const FStageMemberAssignment& Assignment, FText& Error)
{
	AActor* Actor = GetParticipant(Assignment.GetEffectiveRoleId());
	auto* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
	if (!IsValid(Member) || !IsValid(Assignment.Command))
	{
		Error = FText::FromString(TEXT("A required member or command disappeared."));
		return nullptr;
	}
	auto* Working = DuplicateObject<UStageMemberCommand>(Assignment.Command, Member);
	TArray<FString> ParameterErrors;
	if (ActiveStage && !FStageCommandParameters::ApplyTree(Working, ActiveStage->Parameters, ParameterErrors, ActiveProduction ? &ActiveProduction->Parameters : nullptr))
	{
		Error = FText::FromString(FString::Join(ParameterErrors, TEXT("\n")));
		return nullptr;
	}
	FStageExecutionContext Context;
	Context.Manager = this;
	Context.MemberComponent = Member;
	Working->PreStart(Context);
	TFunction<bool(const UStageMemberCommand*)> CheckDuration = [&](const UStageMemberCommand* Command)
	{
		if (!IsValid(Command)) return false;
		for (const UStageMemberCommand* Child : Command->GetChildCommands())
			if (!CheckDuration(Child)) return false;
		const float Duration = Command->GetNeededTime();
		return FMath::IsFinite(Duration) && Duration >= 0.0f;
	};
	if (!CheckDuration(Working))
	{
		Error = FText::FromString(TEXT("Prepared command durations must be finite and nonnegative."));
		return nullptr;
	}
	return Working;
}

TArray<TObjectPtr<UStageMemberCommand>> AStageManager::PrepareCommands(
    const TArray<FStageMemberAssignment>& Assignments, EStageCommandType Type, int32 SourceIndex, FGuid ReactionActivation)
{
	TArray<TObjectPtr<UStageMemberCommand>> Prepared;
	for (const auto& Assignment : Assignments)
	{
		AActor* Actor = GetParticipant(Assignment.GetEffectiveRoleId());
		auto* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
		if (!IsValid(Member) || !IsValid(Assignment.Command))
		{
			FailStage(FText::FromString(TEXT("A required member or command disappeared.")));
			return {};
		}
		FText Error;
		auto* Working = Type == EStageCommandType::Scenario ? Assignment.Command.Get() : CreatePreparedCommand(Assignment, Error);
		if (!Working)
		{
			FailStage(Error);
			return {};
		}
		FStageExecutionContext Context;
		Context.Manager = this;
		Context.MemberComponent = Member;
		const float Start = Type == EStageCommandType::Scenario ? PlannedStepStarts[SourceIndex] : GetStageElapsedTime();
		const float End = Start + Working->GetNeededTime();
		if (!FMath::IsFinite(End))
		{
			FailStage(FText::FromString(TEXT("Command plan duration overflow.")));
			return {};
		}
		Working->ResetRuntime(Context, Working, Start, End);
		Working->OnFinished.AddDynamic(this, &AStageManager::HandleRootFinished);
		FStageRootExecution& Entry = RootExecutions.AddDefaulted_GetRef();
		Entry.Command = Working;
		Entry.MemberTag = Assignment.MemberTag;
		Entry.RoleId = Assignment.GetEffectiveRoleId();
		Entry.MemberComponent = Member;
		Entry.CommandType = Type;
		Entry.SourceIndex = SourceIndex;
		Entry.ReactionActivationId = ReactionActivation;
		Prepared.Add(Working);
	}
	return Prepared;
}

void AStageManager::IssueCommands(const TArray<TObjectPtr<UStageMemberCommand>>& Commands)
{
	for (UStageMemberCommand* Command : Commands)
	{
		const auto* Entry = FindRoot(Command);
		if (!Entry || !Entry->MemberComponent.IsValid()) continue;
		auto* Member = Entry->MemberComponent.Get();
		const auto Type = Entry->CommandType;
		if (!Member->RunCommand(Command, Type)) Member->CompleteBeforeStart(Command, EStageCommandResult::Cancelled);
	}
}

const FStageStep* AStageManager::FindPreparedStep(FName StepId) const
{
	const int32 Index = ActiveStage ? ActiveStage->FindStepIndex(StepId) : INDEX_NONE;
	return PreparedSteps.IsValidIndex(Index) ? &PreparedSteps[Index] : nullptr;
}

FStageRootExecution* AStageManager::FindRoot(const UStageMemberCommand* Command)
{
	const auto* Root = Command ? Command->GetRootCommand() : nullptr;
	return RootExecutions.FindByPredicate([Root](const auto& Entry)
	{
		return Entry.Command == Root;
	});
}

const FStageRootExecution* AStageManager::FindRoot(const UStageMemberCommand* Command) const
{
	const auto* Root = Command ? Command->GetRootCommand() : nullptr;
	return RootExecutions.FindByPredicate([Root](const auto& Entry)
	{
		return Entry.Command == Root;
	});
}

void AStageManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshSceneBriefing();
	TickProduction();
	if (State != EStageState::Idle)
	{
		for (const auto& Bound : BoundMembers)
			if (!Bound.IsValid() || !IsValid(Bound->GetOwner()) || Bound->GetOwner()->IsActorBeingDestroyed())
			{
				AbortStage(FText::FromString(TEXT("Bound participant disappeared")));
				break;
			}
		if (State == EStageState::Idle)
		{
			UpdateDebugDisplay();
			return;
		}
		UpdateTiming();
		if (State == EStageState::Running && StageElapsedTime >= ActiveStage->MainDuration) BeginFinale();
		if (State == EStageState::Finalizing && GetWorld()->GetTimeSeconds() - FinalizingStartedAt >= ActiveStage->FinaleBudget)
			AbortStage(FText::FromString(TEXT("FinaleBudget expired")));
		if (State == EStageState::Stopping && GetWorld()->GetTimeSeconds() >= StoppingDeadline) AbortStage(FText::FromString(TEXT("Stopping deadline expired")));
		if (bAdvanceRequested) AdvanceStage();
	}
	UpdateDebugDisplay();
}

void AStageManager::AdvanceStage()
{
	if (bIssuingCommands || bAdvancing || EventDispatchDepth != 0)
	{
		bAdvanceRequested = true;
		return;
	}
	bAdvanceRequested = false;
	if (State == EStageState::Running)
	{
		UpdateTiming();
		if (StageElapsedTime >= ActiveStage->MainDuration)
			BeginFinale();
		else if (!bStoryComplete && !CurrentStepResults.FindKey(EStageCommandResult::None))
		{
			TGuardValue<bool> Advancing(bAdvancing, true);
			int32 NextStepIndex = CurrentStepIndex + 1;
			if (ActiveStage->bUseStepLinks && CurrentStepIndex == INDEX_NONE)
			{
				NextStepIndex = ActiveStage->FindStepIndex(ActiveStage->EntryStepId);
			}
			else if (ActiveStage->bUseStepLinks && PreparedSteps.IsValidIndex(CurrentStepIndex))
			{
				const FStageStep& Step = PreparedSteps[CurrentStepIndex];
				const bool bTakeFallback = !Step.FailedStepId.IsNone() && CurrentStepResults.FindKey(EStageCommandResult::Failed);
				NextStepIndex = ActiveStage->FindStepIndex(bTakeFallback ? Step.FailedStepId : Step.ThenStepId);
				if (bTakeFallback) AddDiagnostic(TEXT("On Failed -> ") + Step.FailedStepId.ToString());
			}
			while (PreparedSteps.IsValidIndex(NextStepIndex) && PreparedSteps[NextStepIndex].Branch)
			{
				const FStageStep& Step = PreparedSteps[NextStepIndex];
				const UStageCondition* Condition = Step.Branch->Condition;
				const bool bConditionMet = IsValid(Condition) && Condition->Evaluate(this);
				if (State != EStageState::Running) break;
				AddDiagnostic(TEXT("Branch[") + Step.StepId.ToString() + (bConditionMet ? TEXT("] -> True") : TEXT("] -> False")));
				NextStepIndex = ActiveStage->FindStepIndex(bConditionMet ? Step.ThenStepId : Step.FailedStepId);
			}
			if (State != EStageState::Running)
			{
				bAdvanceRequested = true;
				return;
			}
			if (!PreparedSteps.IsValidIndex(NextStepIndex))
			{
				bStoryComplete = true;
				if (ActiveStage->TimingPolicy != EStageTimingPolicy::FixedMainDuration) BeginFinale();
			}
			else
			{
				TGuardValue<bool> Issuing(bIssuingCommands, true);
				PreviousStepResults = MoveTemp(CurrentStepResults);
				CurrentStepResults.Reset();
				CurrentStepIndex = NextStepIndex;
				PlayerCues->NotifyStepStarted(PreparedSteps[CurrentStepIndex].StepId);
				const auto& Assignments = PreparedSteps[CurrentStepIndex].Commands;
				for (const auto& Assignment : Assignments)
					CurrentStepResults.Add(Assignment.GetEffectiveRoleId(), EStageCommandResult::None);
				const auto Prepared = PrepareCommands(Assignments, EStageCommandType::Scenario, CurrentStepIndex);
				IssueCommands(Prepared);
			}
		}
	}
	FinishStageIfReady();
}

void AStageManager::BeginFinale()
{
	if (State != EStageState::Running) return;
	UpdateTiming();
	State = EStageState::Finalizing;
	FinalizingStartedAt = GetWorld()->GetTimeSeconds();
	PlayerCues->NotifyStepStarted(NAME_None);
	AddDiagnostic(TEXT("Finale started"));
	bIssuingCommands = true;
	const auto Prepared = PrepareCommands(ActiveStage->Finale, EStageCommandType::Finale, INDEX_NONE);
	const auto Snapshot = BoundMembers;
	for (const auto& Member : Snapshot)
		if (Member.IsValid()) Member->RequestStopCommands();
	IssueCommands(Prepared);
	bIssuingCommands = false;
	bAdvanceRequested = true;
}

void AStageManager::FailStage(const FText& Reason)
{
	if (State == EStageState::Idle || State == EStageState::Stopping) return;
	UpdateTiming();
	State = EStageState::Stopping;
	FailureReason = Reason;
	float Budget = 30.0f;
	for (const auto& Entry : RootExecutions)
		if (Entry.Command && Entry.Result == EStageCommandResult::None) Budget = FMath::Max(Budget, Entry.Command->GetRemainingTime() * 2.0f + 10.0f);
	StoppingDeadline = GetWorld()->GetTimeSeconds() + Budget;
	const bool bWasIssuing = bIssuingCommands;
	bIssuingCommands = true;
	const auto Snapshot = BoundMembers;
	for (const auto& Member : Snapshot)
		if (Member.IsValid()) Member->EndStageMember();
	bIssuingCommands = bWasIssuing;
	bAdvanceRequested = true;
}

void AStageManager::FinishStageIfReady()
{
	if ((State != EStageState::Finalizing && State != EStageState::Stopping) || bIssuingCommands || bAdvancing || EventDispatchDepth != 0) return;
	if (State == EStageState::Finalizing)
	{
		for (const auto& Entry : RootExecutions)
		{
			if (Entry.Result == EStageCommandResult::None) return;
			if (Entry.CommandType == EStageCommandType::Finale && Entry.Result != EStageCommandResult::Succeeded)
			{
				FailStage(FText::FromString(TEXT("A mandatory Finale command failed.")));
				return;
			}
		}
		bIssuingCommands = true;
		const auto Snapshot = BoundMembers;
		for (const auto& Member : Snapshot)
			if (Member.IsValid()) Member->EndStageMember();
		bIssuingCommands = false;
	}
	for (const auto& Member : BoundMembers)
		if (Member.IsValid() && !Member->IsMemberFinished()) return;
	const bool bFailed = State == EStageState::Stopping;
	const FGameplayTag StageId = ActiveStage->StageId;
	const FName Key = ActiveStage->GetEffectiveStageKey();
	const FText Reason = FailureReason;
	const FGuid ProductionAtFinish = ProductionRunId;
	StageElapsedTime = GetStageElapsedTime();
	CleanupStage();
	if (StageId.IsValid())
	{
		if (bFailed)
			OnStageFailed.Broadcast(StageId, Reason);
		else
			OnStageCompleted.Broadcast(StageId);
	}
	OnStageFinishedById.Broadcast(Key, !bFailed, Reason);
	if (IsValid(this) && ProductionRunId == ProductionAtFinish && IsProductionActive() && !bAbortingProduction) HandleProductionSceneFinished(Key, !bFailed, Reason);
}

void AStageManager::CleanupStage()
{
	PlayerCues->ResetCues();
	for (auto& Entry : RootExecutions)
		if (IsValid(Entry.Command)) Entry.Command->OnFinished.RemoveDynamic(this, &AStageManager::HandleRootFinished);
	for (const auto& Member : BoundMembers)
		if (Member.IsValid()) Member->OnMemberFinished.RemoveDynamic(this, &AStageManager::HandleMemberFinished);
	RootExecutions.Reset();
	AccountedCommands.Reset();
	Members.Reset();
	BoundMembers.Reset();
	CurrentStepResults.Reset();
	PreviousStepResults.Reset();
	PreparedSteps.Reset();
	PlannedStepStarts.Reset();
	ReactionActivationCounts.Reset();
	StartedReactionActivations.Reset();
	ActiveStage = nullptr;
	ActiveTargetBindings.Reset();
	CurrentStepIndex = INDEX_NONE;
	bAdvanceRequested = false;
	State = EStageState::Idle;
}

void AStageManager::HandleRootFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	auto* Entry = FindRoot(Command);
	if (!Entry || Entry->Command != Command || Entry->Result != EStageCommandResult::None) return;
	Entry->Result = Result;
	AddDiagnostic(DescribeCommand(Command) + TEXT(" / result=") + StaticEnum<EStageCommandResult>()->GetNameStringByValue(static_cast<int64>(Result)));
	if (Entry->CommandType == EStageCommandType::Scenario && Entry->SourceIndex == CurrentStepIndex) CurrentStepResults.FindOrAdd(Entry->RoleId) = Result;
	if (Entry->CommandType == EStageCommandType::Finale && Result != EStageCommandResult::Succeeded) FailStage(FText::FromString(TEXT("A mandatory Finale command failed.")));
	bAdvanceRequested = true;
}

void AStageManager::HandleMemberFinished(UStageMemberComponent* Member)
{
	bAdvanceRequested = true;
}

void AStageManager::BeginNotification()
{
	++EventDispatchDepth;
}

void AStageManager::EndNotification()
{
	check(EventDispatchDepth > 0);
	--EventDispatchDepth;
	if (EventDispatchDepth == 0 && bAdvanceRequested) AdvanceStage();
}

float AStageManager::GetCommandLateness(const UStageMemberCommand* Command) const
{
	if (!ActiveStage || !Command) return 0.0f;
	return FMath::Max(0.0f, GetStageElapsedTime() + Command->GetRemainingTime() - Command->GetPlannedEndTime() - ActiveStage->AdditiveDuration);
}

EStageLatenessLevel AStageManager::ResolveLatenessLevel(float Seconds) const
{
	EStageLatenessLevel Level = EStageLatenessLevel::OnTime;
	float HighestThreshold = 0.0f;
	if (ActiveStage)
		for (const auto& Pair : ActiveStage->LatenessThreshold)
			if (Pair.Value <= Seconds && Pair.Value > HighestThreshold)
			{
				HighestThreshold = Pair.Value;
				Level = Pair.Key;
			}
	return Level;
}

void AStageManager::UpdateTiming()
{
	StageElapsedTime = GetStageElapsedTime();
	if (State != EStageState::Running) return;
	float MeasuredLateness = 0.0f;
	bool bHasUnfinishedLeaf = false;
	TFunction<void(const UStageMemberCommand*)> Measure = [&](const UStageMemberCommand* Command)
	{
		if (!IsValid(Command) || Command->GetState() == EStageExecutionState::Finished) return;
		if (!Command->IsComposite())
		{
			bHasUnfinishedLeaf = true;
			MeasuredLateness = FMath::Max(MeasuredLateness, GetCommandLateness(Command));
		}
		else
			for (const UStageMemberCommand* Child : Command->GetChildCommands())
				Measure(Child);
	};
	for (const auto& Entry : RootExecutions)
		if (Entry.CommandType == EStageCommandType::Scenario && Entry.Result == EStageCommandResult::None) Measure(Entry.Command);
	// Keep the final sample recorded by AccountCommandResult until the next Step or Stage.
	if (bHasUnfinishedLeaf || CurrentStepIndex == INDEX_NONE) CurrentLatenessSeconds = MeasuredLateness;
	MaxLatenessSeconds = FMath::Max(MaxLatenessSeconds, CurrentLatenessSeconds);
	CurrentLatenessLevel = ResolveLatenessLevel(CurrentLatenessSeconds);
}

bool AStageManager::IsReactionAllowedInCurrentStep(const FStageMemberReaction& Reaction) const
{
	if (Reaction.bIsAllowedStepRange)
	{
		if (!ActiveStage || !ActiveStage->Steps.IsValidIndex(CurrentStepIndex)) return false;
		TSet<FName> Range;
		return ActiveStage->GetStepRange(Reaction.AllowedFromStepId, Reaction.AllowedToStepId, Range)
		    && Range.Contains(ActiveStage->Steps[CurrentStepIndex].StepId);
	}
	return Reaction.AllowedStepIds.IsEmpty()
	    || (ActiveStage && ActiveStage->Steps.IsValidIndex(CurrentStepIndex) && Reaction.AllowedStepIds.Contains(ActiveStage->Steps[CurrentStepIndex].StepId));
}

bool AStageManager::CheckReaction(const FStageRootExecution& Entry) const
{
	if (!ActiveStage || !ActiveStage->MemberReactions.IsValidIndex(Entry.SourceIndex) || !Entry.MemberComponent.IsValid()) return false;
	const auto& Reaction = ActiveStage->MemberReactions[Entry.SourceIndex];
	const auto* Statuses = Entry.MemberComponent->GetOwner()->FindComponentByClass<UStatusesComponent>();
	const bool OwnActivation = Entry.ReactionActivationId.IsValid() && StartedReactionActivations.Contains(Entry.ReactionActivationId);
	if (!OwnActivation && !IsReactionAllowedInCurrentStep(Reaction)) return false;
	return Statuses && (OwnActivation || !Reaction.bUseCooldown || GetStageElapsedTime() - LastReactionStartedTimes[Entry.SourceIndex] >= Reaction.CooldownDuration)
	    && (OwnActivation || Reaction.MaxActivationsPerStage == 0 || ReactionActivationCounts[Entry.SourceIndex] < Reaction.MaxActivationsPerStage)
	    && (!Reaction.CooldownTag.IsValid() || !Statuses->GetStatuses().HasTag(Reaction.CooldownTag)) && Reaction.ExecRequirements.GetIsAccess(Statuses->GetStatuses());
}

EStageCommandResult AStageManager::CheckCommandStart(UStageMemberCommand* Command)
{
	if (!ActiveStage) return EStageCommandResult::None;
	if (State == EStageState::Running && GetStageElapsedTime() >= ActiveStage->MainDuration) BeginFinale();
	const auto* Entry = FindRoot(Command);
	if (!Entry || State == EStageState::Stopping || State == EStageState::Idle || (State == EStageState::Finalizing && Entry->CommandType != EStageCommandType::Finale))
		return EStageCommandResult::Cancelled;
	if (Entry->CommandType == EStageCommandType::Scenario && !Command->RequiredSuccessRoleId.IsNone())
	{
		const auto* Result = PreviousStepResults.Find(Command->RequiredSuccessRoleId);
		if (!Result || *Result != EStageCommandResult::Succeeded) return EStageCommandResult::Blocked;
	}
	if (Entry->CommandType == EStageCommandType::Reaction && !Entry->bStarted && !CheckReaction(*Entry)) return EStageCommandResult::Cancelled;
	return EStageCommandResult::None;
}

bool AStageManager::ShouldSkipCommand(const UStageMemberCommand* Command) const
{
	const auto* Entry = FindRoot(Command);
	return Entry && Entry->CommandType == EStageCommandType::Scenario && Command->CatchUpPolicy == EStageCatchUpPolicy::SkipBeforeStart && GetCommandLateness(Command) > 0.0f;
}

void AStageManager::HandleCommandStarted(UStageMemberCommand* Command)
{
	if (auto* Entry = FindRoot(Command); Entry && !Entry->bStarted)
	{
		Entry->bStarted = true;
		if (Entry->CommandType != EStageCommandType::Reaction) return;
		if (!Entry->ReactionActivationId.IsValid() || !StartedReactionActivations.Contains(Entry->ReactionActivationId))
		{
			if (Entry->ReactionActivationId.IsValid()) StartedReactionActivations.Add(Entry->ReactionActivationId);
			++ReactionActivationCounts[Entry->SourceIndex];
			LastReactionStartedTimes[Entry->SourceIndex] = GetStageElapsedTime();
		}

		const auto& Reaction = ActiveStage->MemberReactions[Entry->SourceIndex];
		if (!Reaction.CooldownTag.IsValid()) return;
		const auto* Member = Entry->MemberComponent.Get();
		AActor* Actor = Member ? Member->GetOwner() : nullptr;
		auto* Statuses = IsValid(Actor) ? Actor->FindComponentByClass<UStatusesComponent>() : nullptr;
		if (!Statuses) return;

		FStatusesInfoArray CooldownStatuses;
		CooldownStatuses.StatusesInfo.Emplace(Reaction.CooldownTag.GetSingleTagContainer(), EStatusState::Temporary, Reaction.CooldownDuration);
		CooldownStatuses.StatusesInfo[0].bIsClearTimer = true;
		// Mark bStarted before status callbacks, so this cooldown cannot reject its own reaction admission.
		Statuses->AddStatusesWithInfo(CooldownStatuses);
	}
}

void AStageManager::AccountCommandResult(UStageMemberCommand* Command, EStageCommandResult Result)
{
	const auto* Entry = FindRoot(Command);
	if (!Entry || AccountedCommands.Contains(Command)) return;
	AccountedCommands.Add(Command);
	if (Entry->Command != Command)
		AddDiagnostic(DescribeCommand(Command) + TEXT(" / result=") + StaticEnum<EStageCommandResult>()->GetNameStringByValue(static_cast<int64>(Result)));
	if (State == EStageState::Running && Entry->CommandType == EStageCommandType::Scenario && !Command->IsComposite())
	{
		CurrentLatenessSeconds = FMath::Max(CurrentLatenessSeconds, GetCommandLateness(Command));
		MaxLatenessSeconds = FMath::Max(MaxLatenessSeconds, CurrentLatenessSeconds);
		CurrentLatenessLevel = ResolveLatenessLevel(CurrentLatenessSeconds);
	}
	if (Result != EStageCommandResult::Succeeded || Command->IsComposite() || !Command->HasStartedMainAction()) return;
	const int32 EarnedScore = Command->GetEarnedScore();
	if (EarnedScore <= 0) return;
	float Multiplier = 1.0f;
	if (Entry->CommandType == EStageCommandType::Scenario)
		if (const float* Value = ScoreMultipliers.Find(ResolveLatenessLevel(GetCommandLateness(Command)))) Multiplier = *Value;
	AddScore(FMath::RoundToInt(static_cast<double>(EarnedScore) * Multiplier));
}

void AStageManager::ReceiveStageEvent(const FStageEvent& Event)
{
	if (!Event.EventTag.IsValid() || (State != EStageState::Running && State != EStageState::Finalizing)) return;
	BeginNotification();
	AddDiagnostic(
	    FString::Printf(TEXT("Event[%s] / subject=%s / instigator=%s"), *Event.EventTag.ToString(), *GetNameSafe(Event.SubjectActor), *GetNameSafe(Event.InstigatorActor)));
	OnStageEvent.Broadcast(Event);
	if (State == EStageState::Running)
	{
		const auto Reactions = ActiveStage->MemberReactions;
		for (int32 Index = 0; Index < Reactions.Num() && State == EStageState::Running; ++Index)
		{
			const auto& Reaction = Reactions[Index];
			if (!Event.EventTag.MatchesTag(Reaction.EventTag)
			    || (!Reaction.GetEffectiveSubjectRoleId().IsNone() && GetParticipant(Reaction.GetEffectiveSubjectRoleId()) != Event.SubjectActor))
				continue;
			if (!IsReactionAllowedInCurrentStep(Reaction))
			{
				AddDiagnostic(TEXT("Reaction[") + Reaction.ReactionId.ToString() + TEXT("] ignored: current StepId is not allowed"));
				continue;
			}
			if (Reaction.bRequireSubjectInSight)
			{
				const AActor* Observer = GetParticipant(Reaction.GetEffectiveRoleId());
				const AActor* Subject = Event.SubjectActor;
				if (!IsValid(Observer) || !IsValid(Subject) || Observer == Subject) continue;
				const FVector ToSubject = Subject->GetActorLocation() - Observer->GetActorLocation();
				if (FVector::DotProduct(Observer->GetActorForwardVector().GetSafeNormal2D(), ToSubject.GetSafeNormal2D()) < 0.42) continue;
				FVector Eyes;
				FRotator EyeRotation;
				Observer->GetActorEyesViewPoint(Eyes, EyeRotation);
				FCollisionQueryParams Query(SCENE_QUERY_STAT(StageReactionSight), false, Observer);
				Query.AddIgnoredActor(Subject);
				if (GetWorld()->LineTraceTestByChannel(Eyes, Subject->GetActorLocation(), ECC_Visibility, Query)) continue;
			}
			const auto Assignments = Reaction.GetAssignments();
			bool Ready = true;
			for (const auto& Assignment : Assignments)
			{
				AActor* Actor = GetParticipant(Assignment.GetEffectiveRoleId());
				auto* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
				FStageRootExecution Candidate;
				Candidate.MemberComponent = Member;
				Candidate.SourceIndex = Index;
				if (!Member || Member->HasReaction() || !CheckReaction(Candidate))
				{
					Ready = false;
					break;
				}
			}
			if (!Ready)
			{
				AddDiagnostic(TEXT("Reaction[") + Reaction.ReactionId.ToString() + TEXT("] rejected: performer busy, requirements, cooldown or activation limit"));
				continue;
			}
			AddDiagnostic(TEXT("Reaction[") + Reaction.ReactionId.ToString() + FString::Printf(TEXT("] admitted for %d performers"), Assignments.Num()));
			const auto Prepared = PrepareCommands(Assignments, EStageCommandType::Reaction, Index, FGuid::NewGuid());
			IssueCommands(Prepared);
		}
	}
	EndNotification();
}
