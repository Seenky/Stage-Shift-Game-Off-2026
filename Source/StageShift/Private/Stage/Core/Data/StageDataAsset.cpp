// Fill out your copyright notice in the Description page of Project Settings.

#include "Stage/Core/Data/StageDataAsset.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Stage/Core/StageMemberCommand.h"
#include "Stage/Core/ParallelStageMemberCommand.h"
#include "Stage/Core/Data/StageCommandParameters.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

TArray<FName> UStageDataAsset::GetRoleOptions() const
{
	if (!RoleIds.IsEmpty()) return RoleIds;
	TArray<FName> Result;
	for (const auto& Pair : StageMembers)
		Result.AddUnique(Pair.Key.GetTagName());
	return Result;
}

bool UStageDataAsset::ValidateDefinition(TArray<FString>& Errors, const FInstancedPropertyBag* Overrides) const
{
	const int32 Initial = Errors.Num();
	const FString Root = GetName();
	const auto Error = [&](const FString& Path, const FString& Message)
	{
		Errors.Add(Path + TEXT(" / ") + Message);
	};
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
	TSet<FName> StepIds, ReactionIds, PreviousRoles;
	int32 StepIndex = 0;
	for (const auto& Step : Steps)
	{
		const FString Path = Root + TEXT(" / Step[") + (Step.StepId.IsNone() ? FString::FromInt(StepIndex) : Step.StepId.ToString()) + TEXT("]");
		if (!Step.StepId.IsNone() && StepIds.Contains(Step.StepId)) Error(Path, TEXT("duplicate StepId"));
		StepIds.Add(Step.StepId);
		if (Step.Commands.IsEmpty()) Error(Path, TEXT("step cannot be empty"));
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
	for (const auto& Step : Steps)
		for (const auto& Assignment : Step.Commands)
			Total += Sum(Assignment.Command);
	for (const auto& Assignment : Finale)
		Total += Sum(Assignment.Command);
	EstimatedScore = static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

bool UStageDataAsset::CanSelectParticipant(const AActor* Actor) const
{
	return Actor && Actor->FindComponentByClass<UStageMemberComponent>() != nullptr;
}
