#include "Stage/Commands/PickUpCommand.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/Dialogue/DialogueComponent.h"
#include "Engine/World.h"
#include "Physics/PhysicsActor.h"
#include "Stage/Actions/PickUpAction.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Actions/StageInteractionHelpers.h"

UPickUpCommand::UPickUpCommand()
{
	bRequireStageLocation = true;
	bRequireStageYaw = true;
}

void UPickUpCommand::PostLoad()
{
	Super::PostLoad();
	if (TargetIds.IsEmpty() && !TargetId.IsNone() && !GraphParameterBindings.Contains(TEXT("TargetId")))
	{
		TargetIds.Add(TargetId);
		TargetId = NAME_None;
	}
}

TConstArrayView<FName> UPickUpCommand::GetPickupTargets() const
{
	if (TargetIds.IsEmpty() && !TargetId.IsNone()) return MakeArrayView(&TargetId, 1);
	return TargetIds;
}

bool UPickUpCommand::AcceptsStageProp(const AActor& Prop) const
{
	for (FName Id : GetPickupTargets())
		if (ResolveTarget(Context, Id) == &Prop) return true;
	return false;
}

void UPickUpCommand::PlayWrongPropLine()
{
	if (bWrongPropLineTriggered || WrongPropLine.IsEmpty()) return;
	bWrongPropLineTriggered = true;
	AActor* Owner = StageInteraction::GetOwner(Context);
	if (auto* Dialogue = IsValid(Owner) ? Owner->FindComponentByClass<UDialogueComponent>() : nullptr)
	{
		FDialogueLine Line;
		Line.SpeakersName = FText::GetEmpty();
		Line.Text = WrongPropLine;
		Line.HoldDuration = 0.6f;
		Line.bCheatOut = false;
		Dialogue->PlayOptionalLine(Line);
	}
}

void UPickUpCommand::ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateDefinition(Data, Path, Errors);
	const TConstArrayView<FName> Targets = GetPickupTargets();
	if (Targets.IsEmpty()) Errors.Add(Path + TEXT(" / Pick Up requires at least one Target Id"));
	for (const FName Id : Targets)
		if (Id.IsNone() || !Data || !Data->TargetIds.Contains(Id)) Errors.Add(Path + TEXT(" / undeclared Pick Up target: ") + Id.ToString());
	if (!FMath::IsFinite(MaxDistance) || MaxDistance <= 0) Errors.Add(Path + TEXT(" / MaxDistance must be positive and finite"));
	if (!FMath::IsFinite(ThrowWindowLead) || !FMath::IsFinite(ThrowWindowDuration) || ThrowWindowLead < 0.0f || ThrowWindowDuration < 0.0f)
		Errors.Add(Path + TEXT(" / Throw window timings must be finite and non-negative"));
	if (!FMath::IsFinite(CheckDistanceTime) || CheckDistanceTime < 0.01f)
		Errors.Add(Path + TEXT(" / Check Distance Time must be at least 0.01 seconds"));
	if (!FMath::IsFinite(PickUpPeriodTime) || PickUpPeriodTime < 0.01f)
		Errors.Add(Path + TEXT(" / Pick Up Period Time must be at least 0.01 seconds"));
}

void UPickUpCommand::OnStart_Implementation()
{
	bWrongPropLineTriggered = false;
	PropThrowWindow.Reset();
	if (ThrowWindowDuration > 0.0f)
		if (const UWorld* World = GetWorld())
		{
			const double OpensAt = World->GetTimeSeconds() + ThrowWindowLead;
			PropThrowWindow.Emplace(OpensAt, OpensAt + ThrowWindowDuration);
		}
	Super::OnStart_Implementation();
}

void UPickUpCommand::ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const
{
	Super::ValidateContext(InContext, Path, Errors);
	for (const FName Id : GetPickupTargets())
	{
		const auto* Item = Cast<AStagePhysicsActor>(ResolveTarget(InContext, Id));
		if (!Item || Item->GetInteractionType() != EPhysicsInteractionType::Carry)
			Errors.Add(Path + TEXT(" / Pick Up target must be a StagePhysicsActor with Carry interaction: ") + Id.ToString());
	}
	const AActor* Owner = StageInteraction::GetOwner(InContext);
	if (IsValid(Owner) && !Owner->FindComponentByClass<UPhysicsManipulationComponent>()) Errors.Add(Path + TEXT(" / PickUp requires PhysicsManipulationComponent and a configured CarryPoint"));
}

void UPickUpCommand::OnPreStart_Implementation(const FStageExecutionContext& InContext)
{
	NeededTime = 0.0f;
	AActor* Owner = StageInteraction::GetOwner(InContext);
	for (const FName Id : GetPickupTargets())
	{
		const auto* Animation = StageInteraction::FindAnimation(Owner, ResolveTarget(InContext, Id));
		if (!Animation || !Animation->Montage) continue;
		const UAnimMontage* Montage = Animation->Montage;
		if (!Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event) { return Event.Notify && Event.Notify->GetNotifyName() == TEXT("GrabObject"); })) continue;
		const int32 Section = Montage->GetSectionIndex(Animation->StartSection);
		const float Duration = Section == INDEX_NONE ? Montage->GetPlayLength() : Montage->GetSectionLength(Section);
		NeededTime = FMath::Max(NeededTime, Duration);
	}
}

float UPickUpCommand::GetNeededTime_Implementation() const
{
	return NeededTime;
}

bool UPickUpCommand::IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const
{
	const AActor* Owner = StageInteraction::GetOwner(InContext);
	const auto* Manipulator = IsValid(Owner) ? Owner->FindComponentByClass<UPhysicsManipulationComponent>() : nullptr;
	if (!Manipulator || !Manipulator->IsCarrying()) return false;
	for (const FName Id : GetPickupTargets())
		if (StageInteraction::IsHolding(Manipulator, ResolveTarget(InContext, Id))) return true;
	return false;
}

UStageAction* UPickUpCommand::CreateMainAction_Implementation(const FStageExecutionContext& InContext)
{
	if (!InContext.MemberComponent.IsValid() || !InContext.Manager.IsValid()) return nullptr;
	TArray<TWeakObjectPtr<AActor>> Candidates;
	for (const FName Id : GetPickupTargets()) Candidates.Add(ResolveTarget(InContext, Id));
	auto* Action = NewObject<UPickUpAction>(InContext.MemberComponent.Get());
	Action->SetupPickUp(MoveTemp(Candidates), MaxDistance, PropThrowWindow, CheckDistanceTime, PickUpPeriodTime);
	if (const UWorld* World = GetWorld(); PropThrowWindow.IsSet() && World)
		Action->SetExecutionBudget(static_cast<float>(PropThrowWindow->GetUpperBoundValue() - World->GetTimeSeconds()) + 1.0f);
	return Action;
}
