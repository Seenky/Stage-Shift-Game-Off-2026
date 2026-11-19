// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Core/Data/StageDataAsset.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Core/StageMemberCommand.h"
#include "Stage/Core/StageCondition.h"
#include "Stage/Core/ParallelStageMemberCommand.h"
#include "Stage/Core/Data/StageCommandParameters.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "UObject/ObjectSaveContext.h"
#endif

TArray<FName> UStageDataAsset::GetRoleOptions() const
{
	if (!RoleIds.IsEmpty()) return RoleIds;
	TArray<FName> Result;
	for (const auto& Pair : StageMembers)
		Result.AddUnique(Pair.Key.GetTagName());
	return Result;
}

int32 UStageDataAsset::FindStepIndex(FName StepId) const
{
	if (StepId.IsNone()) return INDEX_NONE;
	return Steps.IndexOfByPredicate([StepId](const FStageStep& Step) { return Step.StepId == StepId; });
}

TArray<FName> UStageDataAsset::GetStepOptions() const
{
	TArray<FName> Options;
	for (const FStageStep& Step : Steps)
		if (!Step.StepId.IsNone()) Options.Add(Step.StepId);
	return Options;
}

bool UStageDataAsset::GetStepRange(FName From, FName To, TSet<FName>& OutSteps) const
{
	OutSteps.Reset();
	const int32 First = FindStepIndex(From);
	const int32 Last = FindStepIndex(To);
	if (First == INDEX_NONE || Last == INDEX_NONE) return false;
	if (!bUseStepLinks)
	{
		if (First > Last) return false;
		for (int32 Index = First; Index <= Last; ++Index) OutSteps.Add(Steps[Index].StepId);
		return true;
	}

	TSet<FName> Visited;
	TFunction<bool(FName)> Collect = [&](FName Id)
	{
		if (OutSteps.Contains(Id)) return true;
		if (Id.IsNone() || Visited.Contains(Id)) return false;
		Visited.Add(Id);
		const int32 Index = FindStepIndex(Id);
		if (Index == INDEX_NONE) return false;
		if (Id != To)
		{
			const FStageStep& Step = Steps[Index];
			const bool bThenReachesEnd = Collect(Step.ThenStepId);
			const bool bAlternateReachesEnd = Collect(Step.FailedStepId);
			if (!bThenReachesEnd && !bAlternateReachesEnd) return false;
		}
		OutSteps.Add(Id);
		return true;
	};
	return Collect(From);
}

bool UStageDataAsset::ValidateStepLinks(TArray<FString>& Errors) const
{
	if (!bUseStepLinks) return true;

	const int32 Initial = Errors.Num();
	TMap<FName, int32> Indices;
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		const FName Id = Steps[Index].StepId;
		if (Id.IsNone() || Indices.Contains(Id))
			Errors.Add(GetName() + TEXT(" / linked Steps need unique StepIds"));
		Indices.Add(Id, Index);
	}
	if (!Steps.IsEmpty() && (EntryStepId.IsNone() || !Indices.Contains(EntryStepId)))
		Errors.Add(GetName() + TEXT(" / connect Execute to the first Step"));

	TArray<uint8> State;
	State.Init(0, Steps.Num());
	TFunction<void(int32)> Visit = [&](int32 Index)
	{
		const FStageStep& Step = Steps[Index];
		if (State[Index] == 2) return;
		if (State[Index] == 1)
		{
			Errors.Add(GetName() + TEXT(" / Step link cycle at ") + Step.StepId.ToString());
			return;
		}
		State[Index] = 1;
		for (const FName Next : {Step.ThenStepId, Step.FailedStepId})
		{
			if (Next.IsNone()) continue;
			if (const int32* Target = Indices.Find(Next)) Visit(*Target);
			else Errors.Add(GetName() + TEXT(" / missing Step link target: ") + Next.ToString());
		}
		State[Index] = 2;
	};
	for (int32 Index = 0; Index < Steps.Num(); ++Index) Visit(Index);
	return Errors.Num() == Initial;
}

bool UStageDataAsset::ValidateDefinition(TArray<FString>& Errors, const FInstancedPropertyBag* Overrides) const
{
	const int32 Initial = Errors.Num();
	const FString Root = GetName();
	const auto Error = [&](const FString& Path, const FString& Message)
	{
		Errors.Add(Path + TEXT(" / ") + Message);
	};
	TSet<FName> CueIds;
	for (const FStageCueDefinition& Cue : PlayerCues)
	{
		const FString Path = Root + TEXT(" / PlayerCue[") + Cue.CueId.ToString() + TEXT("]");
		if (Cue.CueId.IsNone() || CueIds.Contains(Cue.CueId) || Cue.Title.IsEmpty()) Error(Path, TEXT("requires a unique local ID and a title"));
		CueIds.Add(Cue.CueId);
		if (!FMath::IsFinite(Cue.TimeLimit) || Cue.TimeLimit < 0.0f) Error(Path, TEXT("TimeLimit must be finite and nonnegative"));
		for (FName Step : {Cue.ShowAtStepId, Cue.ActivateAtStepId, Cue.HideAtStepId})
			if (!Step.IsNone() && FindStepIndex(Step) == INDEX_NONE) Error(Path, TEXT("unknown StepId: ") + Step.ToString());
		if (!Cue.SubjectRoleId.IsNone() && !GetRoleOptions().Contains(Cue.SubjectRoleId)) Error(Path, TEXT("unknown event subject role"));
		if (Cue.Targets.Num() > 3) Error(Path, TEXT("at most three world markers per cue"));
		for (const FStageCueTarget& Target : Cue.Targets)
		{
			if (!TargetIds.Contains(Target.BindingId) && !GetRoleOptions().Contains(Target.BindingId)) Error(Path, TEXT("unknown marker binding: ") + Target.BindingId.ToString());
			if (!FMath::IsFinite(Target.Radius) || Target.Radius < 10.0f || Target.Offset.ContainsNaN()) Error(Path, TEXT("invalid marker radius or offset"));
		}
		if (Cue.CompletionCondition) Cue.CompletionCondition->ValidateDefinition(this, Path, Errors);
	}
	if (GetEffectiveStageKey().IsNone()) Error(Root, TEXT("StageKey is required"));
	if (TimingPolicy > EStageTimingPolicy::FixedMainDuration) Error(Root, TEXT("unknown timing policy"));
	if (!FMath::IsFinite(MainDuration) || MainDuration <= 0 || !FMath::IsFinite(AdditiveDuration) || AdditiveDuration < 0 || !FMath::IsFinite(FinaleBudget) || FinaleBudget <= 0)
		Error(Root, TEXT("durations must be finite; MainDuration/FinaleBudget positive, AdditiveDuration nonnegative"));
	const auto Unique = [&](const TArray<FName>& Ids, const TCHAR* Kind)
	{
		TSet<FName> Seen;
		for (FName Id : Ids)
		{
			if (Id.IsNone() || Seen.Contains(Id)) Error(Root + TEXT(" / ") + Kind, TEXT("empty or duplicate ID: ") + Id.ToString());
			Seen.Add(Id);
		}
	};
	const TArray<FName> Roles = GetRoleOptions();
	Unique(Roles, TEXT("Roles"));
	Unique(TargetIds, TEXT("Targets"));
	if (Roles.IsEmpty()) Error(Root, TEXT("at least one role is required"));
	if (Steps.IsEmpty()) Error(Root, TEXT("Steps cannot be empty"));
	ValidateStepLinks(Errors);
	if (Finale.IsEmpty()) Error(Root, TEXT("Finale cannot be empty"));
	float PreviousSeconds = 0;
	for (int32 Level = 0; Level <= static_cast<int32>(EStageLatenessLevel::Hard); ++Level)
		if (const float* Value = LatenessThreshold.Find(static_cast<EStageLatenessLevel>(Level)))
		{
			if (!FMath::IsFinite(*Value) || (Level == 0 ? *Value != 0 : *Value <= PreviousSeconds)) Error(Root, TEXT("lateness thresholds must increase from zero"));
			PreviousSeconds = *Value;
		}
	for (const auto& Pair : LatenessThreshold)
		if (Pair.Key > EStageLatenessLevel::Hard) Error(Root, TEXT("unknown lateness level"));
	TSet<const UStageMemberCommand*> Seen;
	TFunction<void(const UStageMemberCommand*, const UObject*, const FString&, const TSet<FName>*)> Check;
	Check = [&](const UStageMemberCommand* Command, const UObject* Parent, const FString& Path, const TSet<FName>* Previous)
	{
		if (!IsValid(Command) || Command->GetClass()->HasAnyClassFlags(CLASS_Abstract))
		{
			Error(Path, TEXT("null or abstract command"));
			return;
		}
		if (Seen.Contains(Command))
		{
			Error(Path, TEXT("shared or cyclic command tree"));
			return;
		}
		Seen.Add(Command);
		if (Command->GetOuter() != Parent) Error(Path, TEXT("command has foreign Outer"));
		const UStageMemberCommand* Definition = Command;
		TStrongObjectPtr<UStageMemberCommand> Effective;
		if (!Command->GraphParameterBindings.IsEmpty())
		{
			Effective.Reset(DuplicateObject<UStageMemberCommand>(Command, GetTransientPackage()));
			if (FStageCommandParameters::ApplyOne(Effective.Get(), Parameters, Errors, Overrides)) Definition = Effective.Get();
		}
		Definition->ValidateDefinition(this, Path, Errors);
		const FName Required = Definition->RequiredSuccessRoleId;
		if (!Required.IsNone() && (!Roles.Contains(Required) || (Previous && !Previous->Contains(Required))))
			Error(Path, TEXT("RequiredSuccessRoleId must belong to the previous step"));
		const auto& Children = Command->GetChildCommands();
		if (Command->IsComposite() && Children.IsEmpty()) Error(Path, TEXT("container cannot be empty"));
		int32 ChildIndex = 0;
		for (const UStageMemberCommand* Child : Children)
			Check(Child, Command, Path + FString::Printf(TEXT(" / Child[%d]"), ChildIndex++), Previous);
		// Resource traversal is safe only after structural validation of the entire subtree.
	};
	TMultiMap<FName, const FStageStep*> Incoming;
	if (bUseStepLinks)
		for (const FStageStep& Step : Steps)
		{
			if (!Step.ThenStepId.IsNone()) Incoming.Add(Step.ThenStepId, &Step);
			if (!Step.FailedStepId.IsNone()) Incoming.AddUnique(Step.FailedStepId, &Step);
		}
	TSet<FName> StepIds, ReactionIds, PreviousRoles;
	int32 StepIndex = 0;
	for (const auto& Step : Steps)
	{
		const FString Path = Root + TEXT(" / Step[") + (Step.StepId.IsNone() ? FString::FromInt(StepIndex) : Step.StepId.ToString()) + TEXT("]");
		if (!Step.StepId.IsNone() && StepIds.Contains(Step.StepId)) Error(Path, TEXT("duplicate StepId"));
		StepIds.Add(Step.StepId);
		if (Step.Branch)
		{
			if (!bUseStepLinks || !Step.Commands.IsEmpty()) Error(Path, TEXT("Branch requires linked Story Steps and cannot contain Commands"));
			const auto* Condition = Step.Branch->Condition.Get();
			if (!IsValid(Condition) || Condition->GetClass()->HasAnyClassFlags(CLASS_Abstract)) Error(Path, TEXT("Branch requires a condition"));
			else if (Step.Branch->GetOuter() != this || Condition->GetOuter() != Step.Branch.Get()) Error(Path, TEXT("Branch has a foreign condition or Outer"));
			else Condition->ValidateDefinition(this, Path, Errors);
			++StepIndex;
			continue;
		}
		if (Step.Commands.IsEmpty()) Error(Path, TEXT("step cannot be empty"));
		if (bUseStepLinks)
		{
			PreviousRoles.Reset();
			bool bFirstPredecessor = true;
			TArray<const FStageStep*> Pending;
			if (Step.StepId != EntryStepId) Incoming.MultiFind(Step.StepId, Pending);
			TSet<const FStageStep*> SeenPredecessors;
			while (!Pending.IsEmpty())
			{
				const FStageStep* Predecessor = Pending.Pop();
				if (SeenPredecessors.Contains(Predecessor)) continue;
				SeenPredecessors.Add(Predecessor);
				if (Predecessor->Branch && Predecessor->StepId != EntryStepId)
				{
					Incoming.MultiFind(Predecessor->StepId, Pending);
					continue;
				}
				TSet<FName> RolesAtPredecessor;
				for (const auto& Assignment : Predecessor->Commands) RolesAtPredecessor.Add(Assignment.GetEffectiveRoleId());
				PreviousRoles = bFirstPredecessor ? MoveTemp(RolesAtPredecessor) : PreviousRoles.Intersect(RolesAtPredecessor);
				bFirstPredecessor = false;
			}
		}
		TSet<FName> Assigned;
		for (const auto& Assignment : Step.Commands)
		{
			const FName Role = Assignment.GetEffectiveRoleId();
			const FString AssignmentPath = Path + TEXT(" / Role[") + Role.ToString() + TEXT("]");
			if (!Roles.Contains(Role) || Assigned.Contains(Role)) Error(AssignmentPath, TEXT("unknown or duplicate role"));
			Assigned.Add(Role);
			Check(Assignment.Command, this, AssignmentPath, &PreviousRoles);
		}
		PreviousRoles = MoveTemp(Assigned);
		++StepIndex;
	}
	TSet<FName> FinaleRoles;
	for (const auto& Assignment : Finale)
	{
		const FName Role = Assignment.GetEffectiveRoleId();
		const FString Path = Root + TEXT(" / Finale / Role[") + Role.ToString() + TEXT("]");
		if (!Roles.Contains(Role) || FinaleRoles.Contains(Role)) Error(Path, TEXT("unknown or duplicate role"));
		FinaleRoles.Add(Role);
		Check(Assignment.Command, this, Path, nullptr);
	}
	int32 ReactionIndex = 0;
	for (const auto& Reaction : MemberReactions)
	{
		const FString Path = Root + TEXT(" / Reaction[") + (Reaction.ReactionId.IsNone() ? FString::FromInt(ReactionIndex) : Reaction.ReactionId.ToString()) + TEXT("]");
		if (!Reaction.ReactionId.IsNone() && ReactionIds.Contains(Reaction.ReactionId)) Error(Path, TEXT("duplicate ReactionId"));
		ReactionIds.Add(Reaction.ReactionId);
		if (!Reaction.EventTag.IsValid()) Error(Path, TEXT("EventTag is required"));
		if (!Roles.Contains(Reaction.GetEffectiveRoleId())) Error(Path, TEXT("unknown performer role"));
		if (!Reaction.GetEffectiveSubjectRoleId().IsNone() && !Roles.Contains(Reaction.GetEffectiveSubjectRoleId())) Error(Path, TEXT("unknown subject role"));
		if (Reaction.bIsAllowedStepRange)
		{
			TSet<FName> Range;
			if (!GetStepRange(Reaction.AllowedFromStepId, Reaction.AllowedToStepId, Range))
				Error(Path, TEXT("Allowed Step Range needs a route from ") + Reaction.AllowedFromStepId.ToString() + TEXT(" to ") + Reaction.AllowedToStepId.ToString());
		}
		else
		{
			for (FName StepId : Reaction.AllowedStepIds)
				if (StepId.IsNone() || !StepIds.Contains(StepId)) Error(Path, TEXT("unknown or empty AllowedStepId: ") + StepId.ToString());
		}
		if (Reaction.MaxActivationsPerStage < 0
		    || ((Reaction.bUseCooldown || Reaction.CooldownTag.IsValid()) && (!FMath::IsFinite(Reaction.CooldownDuration) || Reaction.CooldownDuration <= 0)))
			Error(Path, TEXT("invalid reaction cooldown or activation limit"));
		TSet<FName> Performers;
		for (const auto& Assignment : Reaction.GetAssignments())
		{
			const FName RoleId = Assignment.GetEffectiveRoleId();
			if (!Roles.Contains(RoleId) || Performers.Contains(RoleId)) Error(Path, TEXT("unknown or duplicate reaction performer role"));
			Performers.Add(RoleId);
			Check(Assignment.Command, this, Path + TEXT(" / Role[") + RoleId.ToString() + TEXT("]"), nullptr);
		}
		++ReactionIndex;
	}
	if (Errors.Num() == Initial)
	{
		TFunction<void(const UStageMemberCommand*, const FString&)> Resources;
		Resources = [&](const UStageMemberCommand* Command, const FString& Path)
		{
			uint8 Used = 0;
			for (const UStageMemberCommand* Child : Command->GetChildCommands())
			{
				Resources(Child, Path + TEXT(" / ") + Child->GetClass()->GetName());
				const uint8 Mask = Child->GetResourceMask();
				if (Command->IsA<UParallelStageMemberCommand>() && (Used & Mask)) Error(Path, TEXT("parallel branches conflict on Movement, Rotation or Speech"));
				Used |= Mask;
			}
		};
		for (const auto* Command : Seen)
			Resources(Command, Root + TEXT(" / ") + Command->GetName());
	}
	return Errors.Num() == Initial;
}

#if WITH_EDITOR
void UStageDataAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	// Clipboard imports can propagate RF_Transient to instanced Commands. Existing
	// asset-owned templates must remain serializable, including their nested Commands.
	TSet<UStageMemberCommand*> Visited;
	TFunction<void(UStageMemberCommand*)> Prepare = [&](UStageMemberCommand* Command)
	{
		if (!IsValid(Command) || Visited.Contains(Command) || Command->GetTypedOuter<UStageDataAsset>() != this) return;
		Visited.Add(Command);
		Command->ClearFlags(RF_Transient);
		for (UStageMemberCommand* Child : Command->GetChildCommands()) Prepare(Child);
	};
	const auto PrepareBranch = [this](UStageBranch* Branch)
	{
		if (!IsValid(Branch) || Branch->GetOuter() != this) return;
		Branch->ClearFlags(RF_Transient);
		if (IsValid(Branch->Condition) && Branch->Condition->GetOuter() == Branch) Branch->Condition->ClearFlags(RF_Transient);
	};
	for (const auto& Step : Steps)
	{
		PrepareBranch(Step.Branch);
		for (const auto& Assignment : Step.Commands) Prepare(Assignment.Command);
	}
	for (const auto& Assignment : Finale) Prepare(Assignment.Command);
	for (const auto& Reaction : MemberReactions)
		for (const auto& Assignment : Reaction.GetAssignments()) Prepare(Assignment.Command);
#if WITH_EDITORONLY_DATA
	for (const auto& Draft : EditorDrafts)
	{
		PrepareBranch(Draft.Step.Branch);
		for (const auto& Assignment : Draft.Step.Commands) Prepare(Assignment.Command);
		for (const auto& Assignment : Draft.Reaction.GetAssignments()) Prepare(Assignment.Command);
	}
#endif
	Super::PreSave(SaveContext);
}

EDataValidationResult UStageDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FString> Errors;
	ValidateDefinition(Errors);
	for (const auto& Error : Errors)
		Context.AddError(FText::FromString(Error));
	return Errors.IsEmpty() ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif

void UStageDataAsset::RecalculateEstimatedScore()
{
	TSet<const UStageMemberCommand*> Visiting;
	TFunction<int64(const UStageMemberCommand*)> Sum = [&](const UStageMemberCommand* Command) -> int64
	{
		if (!IsValid(Command) || Visiting.Contains(Command)) return 0;
		Visiting.Add(Command);
		int64 Value = Command->IsComposite() ? 0 : FMath::Max(0, Command->ScoreValue);
		for (const UStageMemberCommand* Child : Command->GetChildCommands())
			Value += Sum(Child);
		Visiting.Remove(Command);
		return Value;
	};
	int64 Total = 0;
	TSet<int32> CountedSteps;
	for (int32 Index = bUseStepLinks ? FindStepIndex(EntryStepId) : 0; Steps.IsValidIndex(Index) && !CountedSteps.Contains(Index);
	     Index = bUseStepLinks ? FindStepIndex(Steps[Index].ThenStepId) : Index + 1)
	{
		CountedSteps.Add(Index);
		for (const auto& Assignment : Steps[Index].Commands)
			Total += Sum(Assignment.Command);
	}
	for (const auto& Assignment : Finale)
		Total += Sum(Assignment.Command);
	EstimatedScore = static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

bool UStageDataAsset::CanSelectParticipant(const AActor* Actor) const
{
	return Actor && Actor->FindComponentByClass<UStageMemberComponent>() != nullptr;
}
