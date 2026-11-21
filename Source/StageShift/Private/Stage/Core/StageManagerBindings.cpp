#include "Stage/Core/StageManager.h"
#include "Camera/CameraComponent.h"
#include "Core/MainCameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Stage/Environment/StageTurntable.h"
#include "Stage/Core/StageDebugDisplay.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageRecoveryRulesDataAsset.h"
#include "Stage/Core/StageMemberCommand.h"
#include "Components/Stage/StageMemberComponent.h"
#include "StatusesComponent.h"
#include "Engine/World.h"
#include "UObject/Package.h"

TMap<FName, AActor*> AStageManager::ResolveBindings(UStageDataAsset* Data, const TMap<FName, AActor*>& Overrides) const
{
	TMap<FName, AActor*> Result;
	if (!Data || !GetWorld()) return Result;
	const TArray<FName> Roles = Data->GetRoleOptions();
	for (const auto& Pair : Data->StageMembers)
	{
		if (!Roles.Contains(Pair.Key.GetTagName())) continue;
		FSoftObjectPath Path = Pair.Value.ToSoftObjectPath();
		if (GetWorld()->WorldType == EWorldType::PIE) Path.FixupForPIE(GetWorld()->GetPackage()->GetPIEInstanceID());
		Result.Add(Pair.Key.GetTagName(), Cast<AActor>(Path.ResolveObject()));
	}
	for (const auto& Pair : RoleBindings)
		if (Roles.Contains(Pair.Key)) Result.Add(Pair.Key, Pair.Value);
	for (const auto& Pair : Overrides)
		if (Roles.Contains(Pair.Key)) Result.Add(Pair.Key, Pair.Value);
	return Result;
}

AActor* AStageManager::GetParticipant(FName RoleId) const
{
	const auto* Actor = Members.Find(RoleId);
	return Actor ? Actor->Get() : nullptr;
}

AActor* AStageManager::GetTarget(FName TargetId) const
{
	const auto& Bindings = bUseValidationTargets ? ValidationTargets : ActiveStage ? ActiveTargetBindings : TargetBindings;
	const auto* Binding = Bindings.Find(TargetId);
	AActor* Actor = Binding ? Binding->Get() : nullptr;
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == GetWorld() ? Actor : nullptr;
}

void AStageManager::SynchronizeBindings()
{
	if (!ConfiguredStage) return;
#if WITH_EDITOR
	Modify();
#endif
	for (FName RoleId : ConfiguredStage->GetRoleOptions())
		RoleBindings.FindOrAdd(RoleId);
	for (FName Target : ConfiguredStage->TargetIds)
		TargetBindings.FindOrAdd(Target);
}

bool AStageManager::CollectValidationErrors(UStageDataAsset* Data, const TMap<FName, AActor*>& Actors, TArray<FString>& Errors, bool bStrictRecovery) const
{
	const int32 InitialCount = Errors.Num();
	if (!IsValid(Data))
	{
		Errors.Add(TEXT("ConfiguredStage / scenario is required"));
		return false;
	}
	Data->ValidateDefinition(Errors);
	const FString Prefix = Data->GetName();
	TSet<AActor*> Seen;
	for (FName RoleId : Data->GetRoleOptions())
	{
		const FString Path = Prefix + TEXT(" / RoleId[") + RoleId.ToString() + TEXT("]");
		AActor* const* Binding = Actors.Find(RoleId);
		AActor* Actor = Binding ? *Binding : nullptr;
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld())
		{
			Errors.Add(Path + TEXT(" / binding requires an actor in the current world"));
			continue;
		}
		if (Seen.Contains(Actor)) Errors.Add(Path + TEXT(" / actor is already bound to another role"));
		Seen.Add(Actor);
		const auto* Member = Actor->FindComponentByClass<UStageMemberComponent>();
		if (!Member || !Actor->FindComponentByClass<UStatusesComponent>())
		{
			Errors.Add(Path + TEXT(" / StageMember and Statuses components are required"));
			continue;
		}
		if (!Member->IsMemberFinished()) Errors.Add(Path + TEXT(" / participant is already executing a stage"));
		if (!FMath::IsFinite(Member->StageLocationTolerance) || Member->StageLocationTolerance < 0 || !FMath::IsFinite(Member->StageYawTolerance) || Member->StageYawTolerance < 0
		    || Member->StageYawTolerance > 180)
			Errors.Add(Path + TEXT(" / invalid stage mark tolerances"));
		if (bStrictRecovery && Member->RecoveryRulesAsset) Member->RecoveryRulesAsset->ValidateDefinition(Errors, Path + TEXT(" / Recovery"));
	}
	for (FName Target : Data->TargetIds)
		if (!GetTarget(Target)) Errors.Add(Prefix + TEXT(" / Target[") + Target.ToString() + TEXT("] / missing current-world actor"));
	TSet<const UStageMemberCommand*> Visited;
	TFunction<void(const UStageMemberCommand*, FName, const FString&)> Check;
	Check = [&](const UStageMemberCommand* Command, FName RoleId, const FString& Path)
	{
		if (!IsValid(Command) || Visited.Contains(Command)) return;
		Visited.Add(Command);
		AActor* const* Binding = Actors.Find(RoleId);
		AActor* Actor = Binding ? *Binding : nullptr;
		if (!IsValid(Actor)) return;
		FStageExecutionContext Context;
		Context.Manager = const_cast<AStageManager*>(this);
		Context.MemberComponent = Actor->FindComponentByClass<UStageMemberComponent>();
		Command->ValidateContext(Context, Path, Errors);
		int32 Index = 0;
		for (const UStageMemberCommand* Child : Command->GetChildCommands())
			Check(Child, RoleId, Path + FString::Printf(TEXT(" / Child[%d]"), Index++));
	};
	int32 Index = 0;
	for (const auto& Step : Data->Steps)
	{
		const FString StepPath = Prefix + TEXT(" / Step[") + (Step.StepId.IsNone() ? FString::FromInt(Index) : Step.StepId.ToString()) + TEXT("]");
		for (const auto& Assignment : Step.Commands)
			Check(Assignment.Command, Assignment.GetEffectiveRoleId(), StepPath + TEXT(" / RoleId[") + Assignment.GetEffectiveRoleId().ToString() + TEXT("]"));
		++Index;
	}
	for (const auto& Assignment : Data->Finale)
		Check(Assignment.Command, Assignment.GetEffectiveRoleId(), Prefix + TEXT(" / Finale / ") + Assignment.GetEffectiveRoleId().ToString());
	for (const auto& Reaction : Data->MemberReactions)
		for (const auto& A : Reaction.GetAssignments())
			Check(A.Command, A.GetEffectiveRoleId(),
			    Prefix + TEXT(" / Reaction[") + Reaction.ReactionId.ToString() + TEXT("] / Role[") + A.GetEffectiveRoleId().ToString() + TEXT("]"));
	int32 Previous = -1;
	for (int32 Threshold : StarThresholds)
	{
		if (Threshold < 0 || Threshold <= Previous) Errors.Add(Prefix + TEXT(" / StarThresholds must increase"));
		Previous = Threshold;
	}
	for (const auto& Pair : ScoreMultipliers)
		if (Pair.Key > EStageLatenessLevel::Hard || !FMath::IsFinite(Pair.Value) || Pair.Value < 0 || Pair.Value > 1
		    || (Pair.Key == EStageLatenessLevel::OnTime && Pair.Value != 1))
			Errors.Add(Prefix + TEXT(" / invalid ScoreMultipliers"));
	return Errors.Num() == InitialCount;
}

void AStageManager::ValidateConfiguredStage()
{
	TArray<FString> Errors;
	CollectValidationErrors(ConfiguredStage, ResolveBindings(ConfiguredStage, {}), Errors);
	for (const auto& Error : Errors)
		UE_LOG(LogTemp, Error, TEXT("Stage validation: %s"), *Error);
	if (Errors.IsEmpty()) UE_LOG(LogTemp, Display, TEXT("Stage validation passed: %s"), *GetNameSafe(ConfiguredStage));
}

bool AStageManager::StartConfiguredStage(FText& Error)
{
	Error = FText::GetEmpty();
	if (bManagerEndingPlay || State != EStageState::Idle || IsProductionActive() || !PendingSceneBriefing.IsEmpty())
	{
		Error = FText::FromString(TEXT("The manager is already running or waiting for a scene."));
		return false;
	}
	TArray<FString> Errors;
	CollectValidationErrors(ConfiguredStage, ResolveBindings(ConfiguredStage, {}), Errors);
	if (!Errors.IsEmpty())
	{
		Error = FText::FromString(FString::Join(Errors, TEXT("\n")));
		return false;
	}
	return ConfiguredStage->SceneBriefing.IsEmpty() ? StartStageByIds(ConfiguredStage, {}, Error) :
	    QueueSceneBriefing(ConfiguredStage->SceneBriefing, Error);
}

AStageTurntable* AStageManager::ResolveTurntable() const
{
	const UWorld* World = GetWorld();
	if (!World || Turntable.IsNull()) return nullptr;
	FSoftObjectPath Path = Turntable.ToSoftObjectPath();
	if (World->WorldType == EWorldType::PIE) Path.FixupForPIE(World->GetPackage()->GetPIEInstanceID());
	AStageTurntable* Platform = Cast<AStageTurntable>(Path.ResolveObject());
	return IsValid(Platform) && Platform->GetWorld() == World ? Platform : nullptr;
}

FVector AStageManager::GetCheatOutAudienceDirection() const
{
	const UWorld* World = GetWorld();
	if (!World) return FVector::ZeroVector;
	if (!CheatOutReference.IsNull())
	{
		FSoftObjectPath Path = CheatOutReference.ToSoftObjectPath();
		if (World->WorldType == EWorldType::PIE) Path.FixupForPIE(World->GetPackage()->GetPIEInstanceID());
		const AActor* Reference = Cast<AActor>(Path.ResolveObject());
		if (IsValid(Reference) && !Reference->IsActorBeingDestroyed() && Reference->GetWorld() == World)
			return FRotator(0.0, Reference->GetActorRotation().Yaw, 0.0).Vector();
	}

	const APlayerController* Player = World->GetFirstPlayerController();
	const auto* TheatreCamera = Player ? Cast<AMainCameraActor>(Player->GetViewTarget()) : nullptr;
	return TheatreCamera ? -TheatreCamera->GetCameraComponent()->GetForwardVector().GetSafeNormal2D() : FVector::ZeroVector;
}

void AStageManager::BeginPlay()
{
	RecentEvents.Reset();
	if (AStageTurntable* Platform = ResolveTurntable()) Platform->SetStageManager(this);
#if WITH_EDITOR
	StageDebugDisplay::ClearEditorContent();
#endif
	Super::BeginPlay();
	if (bAutoStart)
	{
		FText Error;
		if (!(ConfiguredProduction ? StartConfiguredProduction(Error) : StartConfiguredStage(Error))) UE_LOG(LogTemp, Error, TEXT("Stage autostart: %s"), *Error.ToString());
	}
}

void AStageManager::EndPlay(const EEndPlayReason::Type Reason)
{
	bManagerEndingPlay = true;
	AbortProduction(FText::FromString(TEXT("Manager EndPlay")));
	AbortStage(FText::FromString(TEXT("Manager EndPlay")));
	RemoveDebugDisplay();
	Super::EndPlay(Reason);
}

void AStageManager::AbortStage(const FText& Reason)
{
	if (State == EStageState::Idle)
	{
		if (IsProductionActive() && !bAbortingProduction) AbortProduction(Reason);
		CloseSceneBriefing();
		return;
	}
	State = EStageState::Stopping;
	FailureReason = Reason;
	AddDiagnostic(TEXT("Abort: ") + Reason.ToString());
	const bool bWasIssuing = bIssuingCommands;
	bIssuingCommands = true;
	const auto Snapshot = BoundMembers;
	for (const auto& Member : Snapshot)
		if (Member.IsValid()) Member->ForceAbortMember(Reason);
	for (auto& Entry : RootExecutions)
		if (Entry.Result == EStageCommandResult::None) Entry.Result = EStageCommandResult::Cancelled;
	bIssuingCommands = bWasIssuing;
	bAdvanceRequested = true;
	FinishStageIfReady();
}
