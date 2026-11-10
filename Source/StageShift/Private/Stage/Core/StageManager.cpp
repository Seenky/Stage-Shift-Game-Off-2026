#include "Stage/Core/StageManager.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageRecoveryRulesDataAsset.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StatusesComponent.h"
#include "UObject/Package.h"

AStageManager::AStageManager()
{
	PrimaryActorTick.bCanEverTick = true;
	ScoreMultipliers.Add(EStageLatenessLevel::OnTime, 1.0f);
}

bool AStageManager::ValidateStage(UStageDataAsset* Data, const TMap<FGameplayTag, AActor*>& Actors, FText& Error) const
{
	const auto Reject = [&Error](const TCHAR* Message) { Error = FText::FromString(Message); return false; };
	if (!IsValid(Data) || !Data->StageId.IsValid() || !FMath::IsFinite(Data->MainDuration) || Data->MainDuration <= 0.0f ||
		!FMath::IsFinite(Data->AdditiveDuration) || Data->AdditiveDuration < 0.0f)
		return Reject(TEXT("Stage requires an ID, positive MainDuration and nonnegative AdditiveDuration."));
	if (Actors.IsEmpty() || Data->Steps.IsEmpty() || Data->Finale.IsEmpty())
		return Reject(TEXT("Stage requires members, Steps and a mandatory Finale."));
	int32 PreviousThreshold = -1;
	for (int32 Threshold : StarThresholds)
	{
		if (Threshold < 0 || Threshold <= PreviousThreshold) return Reject(TEXT("Star thresholds must increase."));
		PreviousThreshold = Threshold;
	}
	for (const auto& Pair : ScoreMultipliers)
		if (Pair.Key > EStageLatenessLevel::Hard || !FMath::IsFinite(Pair.Value) || Pair.Value < 0.0f || Pair.Value > 1.0f ||
			(Pair.Key == EStageLatenessLevel::OnTime && Pair.Value != 1.0f))
			return Reject(TEXT("Score multipliers must be within 0..1; OnTime must equal 1."));
	float PreviousSeconds = 0.0f;
	for (int32 Level = 0; Level <= static_cast<int32>(EStageLatenessLevel::Hard); ++Level)
	{
		const float* Seconds = Data->LatenessThreshold.Find(static_cast<EStageLatenessLevel>(Level));
		if (!Seconds) continue;
		if (!FMath::IsFinite(*Seconds) || (Level == 0 ? *Seconds != 0.0f : *Seconds <= PreviousSeconds))
			return Reject(TEXT("Lateness thresholds must increase; OnTime must equal 0."));
		PreviousSeconds = *Seconds;
	}
	for (const auto& Pair : Data->LatenessThreshold)
		if (Pair.Key > EStageLatenessLevel::Hard) return Reject(TEXT("Invalid lateness level."));
	TSet<AActor*> SeenActors;
	for (const auto& Pair : Actors)
	{
		AActor* Actor = Pair.Value;
		if (!Pair.Key.IsValid() || !IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld() || SeenActors.Contains(Actor))
			return Reject(TEXT("Each role requires a unique valid actor in this world."));
		SeenActors.Add(Actor);
		const auto* Member = Actor->FindComponentByClass<UStageMemberComponent>();
		if (!Member || !Member->IsMemberFinished() || !Actor->FindComponentByClass<UStatusesComponent>())
			return Reject(TEXT("Each actor requires an available StageMemberComponent and StatusesComponent."));
		if (Member->RecoveryRulesAsset && (!FMath::IsFinite(Member->RecoveryRulesAsset->RecoveryWaitTimeout) ||
			Member->RecoveryRulesAsset->RecoveryWaitTimeout <= 0.0f)) return Reject(TEXT("RecoveryWaitTimeout must be positive."));
	}
	TSet<const UStageMemberCommand*> SeenCommands;
	TFunction<bool(const UStageMemberCommand*, const TSet<FGameplayTag>*, const UObject*)> CheckCommand;
	CheckCommand = [&](const UStageMemberCommand* Command, const TSet<FGameplayTag>* PreviousRoles, const UObject* Parent)
	{
		if (!IsValid(Command) || Command->GetClass()->HasAnyClassFlags(CLASS_Abstract) || SeenCommands.Contains(Command) ||
			(Parent && Command->GetOuter() != Parent)) return false;
		SeenCommands.Add(Command);
		const float Duration = Command->GetNeededTime();
		if (!FMath::IsFinite(Duration) || Duration < 0.0f || Command->ScoreValue < 0) return false;
		if (PreviousRoles && Command->RequiredSuccessStepMemberTag.IsValid() &&
			!PreviousRoles->Contains(Command->RequiredSuccessStepMemberTag)) return false;
		if (Command->IsComposite() && Command->GetChildCommands().IsEmpty()) return false;
		for (const UStageMemberCommand* Child : Command->GetChildCommands()) if (!CheckCommand(Child, PreviousRoles, Command)) return false;
		return true;
	};
	TSet<FGameplayTag> PreviousRoles;
	float TotalDuration = 0.0f;
	for (const FStageStep& Step : Data->Steps)
	{
		if (Step.Commands.IsEmpty()) return Reject(TEXT("A Step cannot be empty."));
		TSet<FGameplayTag> Roles;
		float Duration = 0.0f;
		for (const auto& Assignment : Step.Commands)
		{
			if (!Actors.Contains(Assignment.MemberTag) || Roles.Contains(Assignment.MemberTag) ||
				!CheckCommand(Assignment.Command, &PreviousRoles, nullptr))
				return Reject(TEXT("Invalid Step: check roles, trees, durations and previous-Step dependencies."));
			Roles.Add(Assignment.MemberTag);
			Duration = FMath::Max(Duration, Assignment.Command->GetNeededTime());
		}
		TotalDuration += Duration;
		if (!FMath::IsFinite(TotalDuration)) return Reject(TEXT("Stage plan duration overflow."));
		PreviousRoles = MoveTemp(Roles);
	}
	TSet<FGameplayTag> FinaleRoles;
	for (const auto& Assignment : Data->Finale)
	{
		if (!Actors.Contains(Assignment.MemberTag) || FinaleRoles.Contains(Assignment.MemberTag) ||
			!CheckCommand(Assignment.Command, nullptr, nullptr)) return Reject(TEXT("Invalid Finale assignments."));
		FinaleRoles.Add(Assignment.MemberTag);
	}
	for (const auto& Reaction : Data->MemberReactions)
		if (!Reaction.EventTag.IsValid() || !Actors.Contains(Reaction.MemberTag) ||
			(Reaction.SubjectRoleTag.IsValid() && !Actors.Contains(Reaction.SubjectRoleTag)) || Reaction.MaxActivationsPerStage < 1 ||
			!CheckCommand(Reaction.Command, nullptr, nullptr)) return Reject(TEXT("Invalid member reaction."));
	return true;
}

bool AStageManager::StartStage(UStageDataAsset* Data, const TMap<FGameplayTag, AActor*>& Overrides, FText& Error)
{
	Error = FText::GetEmpty();
	if (State != EStageState::Idle || !IsValid(Data) || !GetWorld())
	{
		Error = FText::FromString(TEXT("Manager must be Idle and StageData must be valid."));
		return false;
	}
	TMap<FGameplayTag, AActor*> Resolved;
	for (const auto& Pair : Data->StageMembers)
	{
		FSoftObjectPath Path = Pair.Value.ToSoftObjectPath();
		if (GetWorld()->WorldType == EWorldType::PIE) Path.FixupForPIE(GetWorld()->GetPackage()->GetPIEInstanceID());
		Resolved.Add(Pair.Key, Cast<AActor>(Path.ResolveObject()));
	}
	for (const auto& Pair : Overrides) Resolved.Add(Pair.Key, Pair.Value);
	if (!ValidateStage(Data, Resolved, Error)) return false;
	TArray<UStageMemberComponent*> PreparedMembers;
	for (const auto& Pair : Resolved)
	{
		auto* Member = Pair.Value->FindComponentByClass<UStageMemberComponent>();
		if (!Member->BindToStage(this))
		{
			for (auto* Prepared : PreparedMembers) Prepared->EndStageMember();
			Error = FText::FromString(TEXT("Could not bind Stage members."));
			return false;
		}
		PreparedMembers.Add(Member);
	}
	ActiveStage = Data;
	Members.Reset();
	BoundMembers.Reset();
	for (const auto& Pair : Resolved) Members.Add(Pair.Key, Pair.Value);
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
	FailureReason = FText::GetEmpty();
	StageStartedAt = GetWorld()->GetTimeSeconds();
	StageElapsedTime = CurrentLatenessSeconds = MaxLatenessSeconds = 0.0f;
	CurrentLatenessLevel = EStageLatenessLevel::OnTime;
	CurrentStepIndex = INDEX_NONE;
	PlannedStepStarts.Reset();
	float Start = 0.0f;
	for (const auto& Step : Data->Steps)
	{
		PlannedStepStarts.Add(Start);
		float Duration = 0.0f;
		for (const auto& Assignment : Step.Commands) Duration = FMath::Max(Duration, Assignment.Command->GetNeededTime());
		Start += Duration;
	}
	State = EStageState::Running;
	bAdvanceRequested = true;
	AdvanceStage();
	return true;
}

AActor* AStageManager::GetMember(FGameplayTag Tag) const
{
	const auto* Actor = Members.Find(Tag);
	return Actor ? Actor->Get() : nullptr;
}

float AStageManager::GetStageElapsedTime() const
{
	return State != EStageState::Idle && GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - StageStartedAt) : StageElapsedTime;
}

void AStageManager::AddScore(int32 Amount)
{
	if (Amount > 0) Score = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Score) + Amount, MAX_int32));
}

int32 AStageManager::GetStarCount() const
{
	int32 Count = 0;
	for (int32 Threshold : StarThresholds) if (Score >= Threshold) ++Count;
	return Count;
}

void AStageManager::RecalculateEstimatedScore()
{
	int64 Total = 0;
	for (UStageDataAsset* Data : StageDefinitions)
	{
		if (!IsValid(Data)) continue;
		Data->RecalculateEstimatedScore();
		Total += Data->EstimatedScore;
	}
	EstimatedTotalScore = static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

TArray<TObjectPtr<UStageMemberCommand>> AStageManager::PrepareCommands(const TArray<FStageMemberAssignment>& Assignments,
	EStageCommandType Type, int32 SourceIndex)
{
	TArray<TObjectPtr<UStageMemberCommand>> Prepared;
	for (const auto& Assignment : Assignments)
	{
		AActor* Actor = GetMember(Assignment.MemberTag);
		auto* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
		if (!IsValid(Member) || !IsValid(Assignment.Command))
		{
			FailStage(FText::FromString(TEXT("A required member or command disappeared.")));
			return {};
		}
		auto* Working = DuplicateObject<UStageMemberCommand>(Assignment.Command, Member);
		FStageExecutionContext Context;
		Context.Manager = this;
		Context.MemberComponent = Member;
		const float Start = Type == EStageCommandType::Scenario ? PlannedStepStarts[SourceIndex] : GetStageElapsedTime();
		Working->ResetRuntime(Context, Working, Start, Start + Working->GetNeededTime());
		Working->OnFinished.AddDynamic(this, &AStageManager::HandleRootFinished);
		FStageRootExecution& Entry = RootExecutions.AddDefaulted_GetRef();
		Entry.Command = Working;
		Entry.MemberTag = Assignment.MemberTag;
		Entry.MemberComponent = Member;
		Entry.CommandType = Type;
		Entry.SourceIndex = SourceIndex;
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

FStageRootExecution* AStageManager::FindRoot(const UStageMemberCommand* Command)
{
	const auto* Root = Command ? Command->GetRootCommand() : nullptr;
	return RootExecutions.FindByPredicate([Root](const auto& Entry) { return Entry.Command == Root; });
}

const FStageRootExecution* AStageManager::FindRoot(const UStageMemberCommand* Command) const
{
	const auto* Root = Command ? Command->GetRootCommand() : nullptr;
	return RootExecutions.FindByPredicate([Root](const auto& Entry) { return Entry.Command == Root; });
}

void AStageManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (State == EStageState::Idle) return;
	UpdateTiming();
	if (State == EStageState::Running && StageElapsedTime >= ActiveStage->MainDuration) BeginFinale();
	if (bAdvanceRequested) AdvanceStage();
}

void AStageManager::AdvanceStage()
{
	if (bIssuingCommands || bAdvancing || EventDispatchDepth != 0) { bAdvanceRequested = true; return; }
	bAdvanceRequested = false;
	if (State == EStageState::Running)
	{
		UpdateTiming();
		if (StageElapsedTime >= ActiveStage->MainDuration) BeginFinale();
		else if (!CurrentStepResults.FindKey(EStageCommandResult::None))
		{
			if (CurrentStepIndex + 1 >= ActiveStage->Steps.Num()) BeginFinale();
			else
			{
				bAdvancing = bIssuingCommands = true;
				PreviousStepResults = MoveTemp(CurrentStepResults);
				CurrentStepResults.Reset();
				++CurrentStepIndex;
				const auto& Assignments = ActiveStage->Steps[CurrentStepIndex].Commands;
				for (const auto& Assignment : Assignments) CurrentStepResults.Add(Assignment.MemberTag, EStageCommandResult::None);
				const auto Prepared = PrepareCommands(Assignments, EStageCommandType::Scenario, CurrentStepIndex);
				IssueCommands(Prepared);
				bAdvancing = bIssuingCommands = false;
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
	bIssuingCommands = true;
	const auto Prepared = PrepareCommands(ActiveStage->Finale, EStageCommandType::Finale, INDEX_NONE);
	for (UStageMemberCommand* Command : Prepared)
	{
		FStageExecutionContext Context;
		Context.Manager = this;
		if (const auto* Entry = FindRoot(Command)) Context.MemberComponent = Entry->MemberComponent;
		Command->FreezeFinaleChoice(Context);
	}
	const auto Snapshot = BoundMembers;
	for (const auto& Member : Snapshot) if (Member.IsValid()) Member->RequestStopCommands();
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
	const bool bWasIssuing = bIssuingCommands;
	bIssuingCommands = true;
	const auto Snapshot = BoundMembers;
	for (const auto& Member : Snapshot) if (Member.IsValid()) Member->EndStageMember();
	bIssuingCommands = bWasIssuing;
	bAdvanceRequested = true;
}

void AStageManager::FinishStageIfReady()
{
	if ((State != EStageState::Finalizing && State != EStageState::Stopping) ||
		bIssuingCommands || bAdvancing || EventDispatchDepth != 0) return;
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
		for (const auto& Member : Snapshot) if (Member.IsValid()) Member->EndStageMember();
		bIssuingCommands = false;
	}
	for (const auto& Member : BoundMembers)
		if (Member.IsValid() && !Member->IsMemberFinished()) return;
	const bool bFailed = State == EStageState::Stopping;
	const FGameplayTag StageId = ActiveStage->StageId;
	const FText Reason = FailureReason;
	StageElapsedTime = GetStageElapsedTime();
	CleanupStage();
	if (bFailed) OnStageFailed.Broadcast(StageId, Reason);
	else OnStageCompleted.Broadcast(StageId);
}

void AStageManager::CleanupStage()
{
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
	PlannedStepStarts.Reset();
	ReactionActivationCounts.Reset();
	ActiveStage = nullptr;
	CurrentStepIndex = INDEX_NONE;
	bAdvanceRequested = false;
	State = EStageState::Idle;
}

void AStageManager::HandleRootFinished(UStageMemberCommand* Command, EStageCommandResult Result)
{
	auto* Entry = FindRoot(Command);
	if (!Entry || Entry->Command != Command || Entry->Result != EStageCommandResult::None) return;
	Entry->Result = Result;
	if (Entry->CommandType == EStageCommandType::Scenario && Entry->SourceIndex == CurrentStepIndex)
		CurrentStepResults.FindOrAdd(Entry->MemberTag) = Result;
	if (Entry->CommandType == EStageCommandType::Finale && Result != EStageCommandResult::Succeeded)
		FailStage(FText::FromString(TEXT("A mandatory Finale command failed.")));
	bAdvanceRequested = true;
}

void AStageManager::HandleMemberFinished(UStageMemberComponent* Member) { bAdvanceRequested = true; }
void AStageManager::BeginNotification() { ++EventDispatchDepth; }
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
			if (Pair.Value <= Seconds && Pair.Value > HighestThreshold) { HighestThreshold = Pair.Value; Level = Pair.Key; }
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
		else for (const UStageMemberCommand* Child : Command->GetChildCommands()) Measure(Child);
	};
	for (const auto& Entry : RootExecutions)
		if (Entry.CommandType == EStageCommandType::Scenario && Entry.Result == EStageCommandResult::None) Measure(Entry.Command);
	// Keep the final sample recorded by AccountCommandResult until the next Step or Stage.
	if (bHasUnfinishedLeaf || CurrentStepIndex == INDEX_NONE) CurrentLatenessSeconds = MeasuredLateness;
	MaxLatenessSeconds = FMath::Max(MaxLatenessSeconds, CurrentLatenessSeconds);
	CurrentLatenessLevel = ResolveLatenessLevel(CurrentLatenessSeconds);
}

bool AStageManager::CheckReaction(const FStageRootExecution& Entry) const
{
	if (!ActiveStage || !ActiveStage->MemberReactions.IsValidIndex(Entry.SourceIndex) || !Entry.MemberComponent.IsValid()) return false;
	const auto& Reaction = ActiveStage->MemberReactions[Entry.SourceIndex];
	const auto* Statuses = Entry.MemberComponent->GetOwner()->FindComponentByClass<UStatusesComponent>();
	return Statuses && ReactionActivationCounts[Entry.SourceIndex] < Reaction.MaxActivationsPerStage &&
		Reaction.ExecRequirements.GetIsAccess(Statuses->GetStatuses());
}

EStageCommandResult AStageManager::CheckCommandStart(UStageMemberCommand* Command)
{
	if (!ActiveStage) return EStageCommandResult::None;
	if (State == EStageState::Running && GetStageElapsedTime() >= ActiveStage->MainDuration) BeginFinale();
	const auto* Entry = FindRoot(Command);
	if (!Entry || State == EStageState::Stopping || State == EStageState::Idle ||
		(State == EStageState::Finalizing && Entry->CommandType != EStageCommandType::Finale)) return EStageCommandResult::Cancelled;
	if (Entry->CommandType == EStageCommandType::Scenario && Command->RequiredSuccessStepMemberTag.IsValid())
	{
		const auto* Result = PreviousStepResults.Find(Command->RequiredSuccessStepMemberTag);
		if (!Result || *Result != EStageCommandResult::Succeeded) return EStageCommandResult::Blocked;
	}
	if (Entry->CommandType == EStageCommandType::Reaction && !Entry->bStarted && !CheckReaction(*Entry)) return EStageCommandResult::Cancelled;
	return EStageCommandResult::None;
}

bool AStageManager::ShouldSkipCommand(const UStageMemberCommand* Command) const
{
	const auto* Entry = FindRoot(Command);
	return Entry && Entry->CommandType == EStageCommandType::Scenario &&
		Command->CatchUpPolicy == EStageCatchUpPolicy::SkipBeforeStart && GetCommandLateness(Command) > 0.0f;
}

void AStageManager::HandleCommandStarted(UStageMemberCommand* Command)
{
	if (auto* Entry = FindRoot(Command); Entry && !Entry->bStarted)
	{
		Entry->bStarted = true;
		if (Entry->CommandType == EStageCommandType::Reaction) ++ReactionActivationCounts[Entry->SourceIndex];
	}
}

void AStageManager::AccountCommandResult(UStageMemberCommand* Command, EStageCommandResult Result)
{
	const auto* Entry = FindRoot(Command);
	if (!Entry || AccountedCommands.Contains(Command)) return;
	AccountedCommands.Add(Command);
	if (State == EStageState::Running && Entry->CommandType == EStageCommandType::Scenario && !Command->IsComposite())
	{
		CurrentLatenessSeconds = FMath::Max(CurrentLatenessSeconds, GetCommandLateness(Command));
		MaxLatenessSeconds = FMath::Max(MaxLatenessSeconds, CurrentLatenessSeconds);
		CurrentLatenessLevel = ResolveLatenessLevel(CurrentLatenessSeconds);
	}
	if (Result != EStageCommandResult::Succeeded || Command->IsComposite() || !Command->HasSuccessfulMainAction()) return;
	float Multiplier = 1.0f;
	if (Entry->CommandType == EStageCommandType::Scenario)
		if (const float* Value = ScoreMultipliers.Find(ResolveLatenessLevel(GetCommandLateness(Command)))) Multiplier = *Value;
	AddScore(FMath::RoundToInt(static_cast<double>(Command->ScoreValue) * Multiplier));
}

void AStageManager::ReceiveStageEvent(const FStageEvent& Event)
{
	if (!Event.EventTag.IsValid() || (State != EStageState::Running && State != EStageState::Finalizing)) return;
	BeginNotification();
	OnStageEvent.Broadcast(Event);
	if (State == EStageState::Running)
	{
		const auto Reactions = ActiveStage->MemberReactions;
		for (int32 Index = 0; Index < Reactions.Num() && State == EStageState::Running; ++Index)
		{
			const auto& Reaction = Reactions[Index];
			if (!Event.EventTag.MatchesTag(Reaction.EventTag) ||
				(Reaction.SubjectRoleTag.IsValid() && GetMember(Reaction.SubjectRoleTag) != Event.SubjectActor)) continue;
			AActor* Actor = GetMember(Reaction.MemberTag);
			auto* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
			if (!Member || Member->HasReaction()) continue;
			FStageRootExecution Candidate;
			Candidate.MemberComponent = Member;
			Candidate.SourceIndex = Index;
			if (!CheckReaction(Candidate)) continue;
			FStageMemberAssignment Assignment;
			Assignment.MemberTag = Reaction.MemberTag;
			Assignment.Command = Reaction.Command;
			const auto Prepared = PrepareCommands({Assignment}, EStageCommandType::Reaction, Index);
			IssueCommands(Prepared);
		}
	}
	EndNotification();
}
