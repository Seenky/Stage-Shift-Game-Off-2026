// Fill out your copyright notice in the Description page of Project Settings.


#include "Managers/Stage/Data/StageDataAsset.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Managers/Stage/StageMemberCommand.h"

void UStageDataAsset::RecalculateEstimatedScore()
{
	TSet<const UStageMemberCommand*> Visiting;
	TFunction<int64(const UStageMemberCommand*)> Sum = [&](const UStageMemberCommand* Command) -> int64
	{
		if (!IsValid(Command) || Visiting.Contains(Command)) return 0;
		Visiting.Add(Command);
		int64 Value = Command->IsComposite() ? 0 : FMath::Max(0, Command->ScoreValue);
		for (const UStageMemberCommand* Child : Command->GetChildCommands()) Value += Sum(Child);
		Visiting.Remove(Command);
		return Value;
	};
	int64 Total = 0;
	for (const auto& Step : Steps) for (const auto& Assignment : Step.Commands) Total += Sum(Assignment.Command);
	for (const auto& Assignment : Finale) Total += Sum(Assignment.Command);
	EstimatedScore = static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

bool UStageDataAsset::CanSelectParticipant(const AActor* Actor) const
{
	return Actor && Actor->FindComponentByClass<UStageMemberComponent>() != nullptr;
}
