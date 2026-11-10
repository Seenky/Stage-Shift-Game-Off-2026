#pragma once

#include "Stage/Core/StageMemberCommand.h"

namespace StageCommandContainer
{
	// Owning Outer links form a tree; reject shared, foreign and null children.
	inline bool HasValidChildren(const UStageMemberCommand* Parent)
	{
		const auto& Children = Parent->GetChildCommands();
		if (Children.IsEmpty()) return false;
		TSet<const UStageMemberCommand*> Seen;
		for (const UStageMemberCommand* Child : Children)
		{
			if (!IsValid(Child) || Child->GetOuter() != Parent || Seen.Contains(Child)) return false;
			Seen.Add(Child);
			if (Child->IsComposite() && !HasValidChildren(Child)) return false;
		}
		return true;
	}

	inline float NeededTime(const UStageMemberCommand* Child)
	{
		const float Duration = Child->GetNeededTime();
		return FMath::IsFinite(Duration) ? FMath::Max(0.0f, Duration) : 0.0f;
	}

	inline EStageCommandResult ResolveResult(const TArray<EStageCommandResult>& Results)
	{
		if (Results.IsEmpty()) return EStageCommandResult::Failed;
		if (Results.Contains(EStageCommandResult::None)) return EStageCommandResult::None;
		bool bAnySucceeded = false;
		for (const EStageCommandResult Result : Results)
		{
			if (Result == EStageCommandResult::Succeeded) bAnySucceeded = true;
			else if (Result != EStageCommandResult::Skipped) return EStageCommandResult::Failed;
		}
		return bAnySucceeded ? EStageCommandResult::Succeeded : EStageCommandResult::Skipped;
	}
}
