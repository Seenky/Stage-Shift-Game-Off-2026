#include "Stage/Core/StageManager.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "StatusesComponent.h"
#include "Stage/Environment/StageCurtainRail.h"
#include "Stage/Cues/StageCueComponent.h"
#include "UObject/Package.h"

AStageManager::FResolvedSceneBindings AStageManager::ResolveProductionBindings(const FStageProductionScene& Scene) const
{
	FResolvedSceneBindings Result;
	Result.Roles = RoleBindings;
	Result.Targets = TargetBindings;
	for (const auto& Pair : ResolveBindings(Scene.Scenario, {}))
		if (!Result.Roles.Contains(Pair.Key)) Result.Roles.Add(Pair.Key, Pair.Value);
	if (const auto* Overrides = ProductionBindings.Find(Scene.SceneId))
	{
		for (const auto& Pair : Overrides->Roles)
			Result.Roles.Add(Pair.Key, ResolveLevelActor(Pair.Value.ToSoftObjectPath()));
		for (const auto& Pair : Overrides->Targets)
			Result.Targets.Add(Pair.Key, ResolveLevelActor(Pair.Value.ToSoftObjectPath()));
	}
	if (ActiveProduction && ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex) && &ActiveProduction->Scenes[ProductionSceneIndex] == &Scene)
		for (const auto& Pair : RuntimeCast) Result.Roles.Add(Pair.Key, Pair.Value.Get());
	return Result;
}

bool AStageManager::ValidateProduction(UStageProductionDataAsset* Production, TArray<FString>& Errors) const
{
	if (!Production)
	{
		Errors.Add(TEXT("ConfiguredProduction is required"));
		return false;
	}
	Production->ValidateDefinition(Errors);
	for (const auto& Scene : Production->Scenes)
	{
		if (!Scene.Scenario) continue;
		const auto Bindings = ResolveProductionBindings(Scene);
		TMap<FName, AActor*> Roles;
		for (FName RoleId : Scene.Scenario->GetRoleOptions())
			Roles.Add(RoleId, Bindings.Roles.FindRef(RoleId));
		TGuardValue<bool> Flag(bUseValidationTargets, true);
		TGuardValue<TMap<FName, TObjectPtr<AActor>>> TargetGuard(ValidationTargets, Bindings.Targets);
		TArray<FString> Local;
		bool bSpawnsCast = false;
		if (!CurtainRail.IsNull())
			if (const auto* LevelBindings = ProductionBindings.Find(Scene.SceneId))
				for (const auto& [RoleId, Placement] : LevelBindings->Cast)
				{
					bSpawnsCast |= Placement.ActorClass != nullptr;
					if (!Roles.Contains(RoleId) || !ResolveLevelActor(Placement.BackstageMark.ToSoftObjectPath())
					    || (Placement.ActorClass && !ResolveLevelActor(Placement.StageMark.ToSoftObjectPath())))
						Local.Add(TEXT("Invalid cast marks for role ") + RoleId.ToString());
				}
		// Spawned actors go through normal scene validation at their actual start.
		if (!bSpawnsCast) CollectValidationErrors(Scene.Scenario, Roles, Local, true);
		for (const auto& Error : Local)
			Errors.Add(TEXT("Scene[") + Scene.SceneId.ToString() + TEXT("] / ") + Error);
		if (Scene.bPrepareBefore)
			for (const auto& Placement : Scene.Placements)
				for (FName Id : {Placement.PropId, Placement.MarkId})
				{
					const auto* Actor = Bindings.Targets.Find(Id);
					if (!Actor || !IsValid(Actor->Get()) || Actor->Get()->GetWorld() != GetWorld())
						Errors.Add(TEXT("Scene[") + Scene.SceneId.ToString() + TEXT("] / missing preparation binding: ") + Id.ToString());
				}
	}
	return Errors.IsEmpty();
}

bool AStageManager::StartConfiguredProduction(FText& Error)
{
	return StartProduction(ConfiguredProduction, Error);
}

bool AStageManager::StartProduction(UStageProductionDataAsset* Production, FText& Error)
{
	Error = FText::GetEmpty();
	if (bManagerEndingPlay || bAbortingProduction || IsActorBeingDestroyed() || IsProductionActive() || State != EStageState::Idle || !GetWorld() ||
	    !PendingSceneBriefing.IsEmpty())
	{
		Error = FText::FromString(TEXT("Finish the active scene/production before starting a production"));
		return false;
	}
	TArray<FString> Errors;
	if (!ValidateProduction(Production, Errors))
	{
		Error = FText::FromString(FString::Join(Errors, TEXT("\n")));
		return false;
	}
	ActiveProduction = Production;
	ProductionSceneCount = Production->Scenes.Num();
	ProductionSceneIndex = 0;
	ProductionRunId = FGuid::NewGuid();
	bProductionAdvancePending = false;
	RuntimeCast.Reset();
	AdvanceProduction();
	if (ProductionState == EStageProductionState::Failed)
	{
		Error = FailureReason;
		return false;
	}
	return true;
}

void AStageManager::AdvanceProduction()
{
	if (!ActiveProduction) return;
	if (!ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex))
	{
		FinishProduction(true, FText::GetEmpty());
		return;
	}
	const auto& Scene = ActiveProduction->Scenes[ProductionSceneIndex];
	const bool bShowBriefing = Scene.Scenario && !Scene.Scenario->SceneBriefing.IsEmpty() &&
	    (!RuntimeCurtain.IsValid() || ProductionSceneIndex == 0);
	if (RuntimeCurtain.IsValid() || Scene.bPrepareBefore || bShowBriefing || !IsStageNavigationReady())
	{
		ProductionState = EStageProductionState::Preparing;
		PreparationStartedAt = GetWorld()->GetTimeSeconds();
		FText Error;
		if (RuntimeCurtain.IsValid() && !PrepareCurtainCast(Scene, Error))
		{
			FinishProduction(false, Error);
			return;
		}
		if (bShowBriefing)
		{
			if (RuntimeCurtain.IsValid()) CurtainPhase = ECurtainPhase::Briefing;
			if (!QueueSceneBriefing(Scene.Scenario->SceneBriefing, Error, Scene.Scenario))
			{
				FinishProduction(false, Error);
				return;
			}
		}
		AddDiagnostic(TEXT("Production[") + ActiveProduction->ProductionId.ToString() + TEXT("] / preparing Scene[") + Scene.SceneId.ToString() + TEXT("]"));
		OnProductionPhaseChanged.Broadcast(ProductionState, Scene.SceneId, ProductionSceneIndex);
		if (RuntimeCurtain.IsValid() && ProductionSceneIndex == 0 && !bShowBriefing) BeginCurtainOpening(true);
	}
	else
	{
		FText Error;
		if (!StartProductionScene(Error)) FinishProduction(false, Error);
	}
}

bool AStageManager::StartProductionScene(FText& Error)
{
	if (!ActiveProduction || !ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex))
	{
		Error = FText::FromString(TEXT("Production scene disappeared"));
		return false;
	}
	const auto& Scene = ActiveProduction->Scenes[ProductionSceneIndex];
	if (!Scene.Scenario)
	{
		Error = FText::FromString(TEXT("Production scenario disappeared"));
		return false;
	}
	const auto Bindings = ResolveProductionBindings(Scene);
	TMap<FName, AActor*> Roles;
	for (FName RoleId : Scene.Scenario->GetRoleOptions())
		Roles.Add(RoleId, Bindings.Roles.FindRef(RoleId));
	TGuardValue<bool> StartGuard(bStartingProductionScene, true);
	const FName SceneId = Scene.SceneId;
	const FGuid StartingRun = ProductionRunId;
	ProductionState = EStageProductionState::Playing;
	if (!StartStageWithBindings(Scene.Scenario, Roles, Bindings.Targets, Error, true)) return false;
	if (RuntimeCurtain.IsValid()) CurtainPhase = ECurtainPhase::Playing;
	if (ActiveProduction && ProductionRunId == StartingRun) OnProductionPhaseChanged.Broadcast(ProductionState, SceneId, ProductionSceneIndex);
	return true;
}

void AStageManager::HandleProductionSceneFinished(FName StageKey, bool bSucceeded, const FText& Reason)
{
	if (!ActiveProduction || ProductionState != EStageProductionState::Playing || !ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex)
	    || !ActiveProduction->Scenes[ProductionSceneIndex].Scenario || ActiveProduction->Scenes[ProductionSceneIndex].Scenario->GetEffectiveStageKey() != StageKey)
		return;
	if (!bSucceeded)
	{
		FinishProduction(false, Reason);
		return;
	}
	if (RuntimeCurtain.IsValid())
	{
		ProductionState = EStageProductionState::Preparing;
		CurtainPhase = ECurtainPhase::Closing;
		SetCurtainSignal(true);
		return;
	}
	// Never recursively start another zero-length scene from completion callbacks.
	bProductionAdvancePending = true;
}

TArray<FString> AStageManager::GetPreparationErrors() const
{
	TArray<FString> Errors;
	if (!ActiveProduction || ProductionState != EStageProductionState::Preparing || !ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex))
	{
		Errors.Add(TEXT("The production is not waiting for preparation"));
		return Errors;
	}
	const auto& Scene = ActiveProduction->Scenes[ProductionSceneIndex];
	if (!Scene.Scenario)
	{
		Errors.Add(TEXT("Production scenario disappeared"));
		return Errors;
	}
	if (!IsStageNavigationReady())
		Errors.Add(NSLOCTEXT("StageNavigation", "WaitingForStage", "Дождись остановки сцены и обновления навигации.").ToString());
	const auto Bindings = ResolveProductionBindings(Scene);
	const auto Resolve = [&](FName Id) -> AActor*
	{
		AActor* Actor = Bindings.Targets.FindRef(Id);
		return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == GetWorld() ? Actor : nullptr;
	};
	const auto Fallen = FGameplayTag::RequestGameplayTag(TEXT("State.Member.Fallen"), false);
	for (FName RoleId : Scene.Scenario->GetRoleOptions())
	{
		AActor* Actor = Bindings.Roles.FindRef(RoleId);
		auto* Member = IsValid(Actor) ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
		const auto* Statuses = IsValid(Actor) ? Actor->FindComponentByClass<UStatusesComponent>() : nullptr;
		if (!IsValid(Actor) || Actor->GetWorld() != GetWorld() || !Member || !Member->IsMemberFinished() || !Statuses || Member->IsPhysicallyBlocked()
		    || Statuses->GetStatuses().HasTag(Fallen))
			Errors.Add(TEXT("RoleId[") + RoleId.ToString() + TEXT("] / actor is missing, busy or physically recovering"));
	}
	for (const auto& Placement : Scene.Placements)
	{
		const auto* Prop = Resolve(Placement.PropId);
		const auto* Mark = Resolve(Placement.MarkId);
		const FString Prefix = TEXT("Prop[") + Placement.PropId.ToString() + TEXT("] / ");
		if (!Prop || !Mark)
		{
			Errors.Add(Prefix + TEXT("missing current-world prop/mark"));
			continue;
		}
		if (FVector::DistSquared(Prop->GetActorLocation(), Mark->GetActorLocation()) > FMath::Square(Placement.PositionTolerance))
			Errors.Add(Prefix + TEXT("not at its preparation mark"));
		if (Placement.bCheckYaw && FMath::Abs(FMath::FindDeltaAngleDegrees(Prop->GetActorRotation().Yaw, Mark->GetActorRotation().Yaw)) > Placement.YawTolerance)
			Errors.Add(Prefix + TEXT("wrong heading"));
		if (Placement.bRequireSettled)
		{
			const auto* Root = Cast<UPrimitiveComponent>(Prop->GetRootComponent());
			if (Prop->GetVelocity().Size() > Placement.MaxSpeed
			    || (Root && Root->IsSimulatingPhysics() && Root->GetPhysicsAngularVelocityInDegrees().Size() > Placement.MaxAngularSpeed))
				Errors.Add(Prefix + TEXT("still moving"));
		}
	}
	return Errors;
}

bool AStageManager::ConfirmProductionPreparation(FText& Error)
{
	if (const auto* Curtain = RuntimeCurtain.Get(); Curtain && (CurtainPhase != ECurtainPhase::Opening || !Curtain->IsOpen()))
	{
		Error = NSLOCTEXT("StageProduction", "CurtainClosed", "Сначала открой занавес.");
		return false;
	}
	if (!PendingSceneBriefing.IsEmpty() && !bConfirmingSceneBriefing)
	{
		Error = NSLOCTEXT("StageBriefing", "WaitingForContinue", "Нажми «Продолжить», чтобы начать сцену.");
		return false;
	}
	const auto Errors = GetPreparationErrors();
	if (!Errors.IsEmpty())
	{
		Error = FText::FromString(FString::Join(Errors, TEXT("\n")));
		return false;
	}
	if (!StartProductionScene(Error))
	{
		FinishProduction(false, Error);
		return false;
	}
	Error = FText::GetEmpty();
	return true;
}

FText AStageManager::GetPreparationPrompt() const
{
	return ActiveProduction && ProductionState == EStageProductionState::Preparing && ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex)
	    ? ActiveProduction->Scenes[ProductionSceneIndex].PreparationPrompt
	    : FText::GetEmpty();
}

void AStageManager::TickProduction()
{
	if (RuntimeCurtain.IsValid() && ActiveProduction)
	{
		TickCurtainProduction();
		return;
	}
	if (bProductionAdvancePending && ActiveProduction && State == EStageState::Idle)
	{
		bProductionAdvancePending = false;
		++ProductionSceneIndex;
		AdvanceProduction();
	}
	if (!ActiveProduction || ProductionState != EStageProductionState::Preparing) return;
	if (!PendingSceneBriefing.IsEmpty()) return;
	if (!ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex))
	{
		FinishProduction(false, FText::FromString(TEXT("Production scene disappeared")));
		return;
	}
	const auto& Scene = ActiveProduction->Scenes[ProductionSceneIndex];
	if (Scene.PreparationTimeout > 0 && GetWorld()->GetTimeSeconds() - PreparationStartedAt >= Scene.PreparationTimeout)
	{
		FinishProduction(false, FText::FromString(TEXT("Preparation timeout expired")));
		return;
	}
	if ((Scene.bAutoStartWhenReady || !Scene.bPrepareBefore) && GetPreparationErrors().IsEmpty())
	{
		FText Error;
		ConfirmProductionPreparation(Error);
	}
}

void AStageManager::FinishProduction(bool bSucceeded, const FText& Reason)
{
	if (!ActiveProduction) return;
	CloseSceneBriefing();
	const FName Key = ActiveProduction->ProductionId;
	ProductionState = bSucceeded ? EStageProductionState::Completed : EStageProductionState::Failed;
	FailureReason = Reason;
	ActiveProduction = nullptr;
	ClearCurtainCastMoves();
	if (!bSucceeded)
		for (const auto& Pair : RuntimeCast)
			if (auto* Actor = Pair.Value.Get()) Actor->Destroy();
	RuntimeCast.Reset();
	CurtainPhase = ECurtainPhase::Idle;
	if (RuntimeCurtain.IsValid())
	{
		SetCurtainSignal(false);
		SetTurntableSignal(false);
	}
	OpeningDeadline = NextOpeningPenaltyAt = -1.;
	bProductionAdvancePending = false;
	ProductionSceneIndex = FMath::Min(ProductionSceneIndex, ProductionSceneCount - 1);
	AddDiagnostic(TEXT("Production[") + Key.ToString() + TEXT("] / ") + (bSucceeded ? TEXT("Completed") : Reason.ToString()));
	OnProductionPhaseChanged.Broadcast(ProductionState, NAME_None, ProductionSceneIndex);
	OnProductionFinished.Broadcast(Key, bSucceeded, Reason);
	if (bSucceeded && RuntimeCurtain.IsValid()) ShowProductionResult();
}

void AStageManager::AbortProduction(const FText& Reason)
{
	if (bAbortingProduction || !IsProductionActive()) return;
	TGuardValue<bool> AbortGuard(bAbortingProduction, true);
	if (State != EStageState::Idle) AbortStage(Reason);
	FinishProduction(false, Reason);
}

void AStageManager::SynchronizeProductionBindings()
{
	if (!ConfiguredProduction) return;
#if WITH_EDITOR
	Modify();
#endif
	for (const auto& Scene : ConfiguredProduction->Scenes)
	{
		auto& Bindings = ProductionBindings.FindOrAdd(Scene.SceneId);
		if (Scene.Scenario)
		{
			for (FName Id : Scene.Scenario->GetRoleOptions())
				if (!Bindings.Roles.Contains(Id)) Bindings.Roles.Add(Id, TSoftObjectPtr<AActor>(RoleBindings.FindRef(Id).Get()));
			for (FName Id : Scene.Scenario->TargetIds)
				if (!Bindings.Targets.Contains(Id)) Bindings.Targets.Add(Id, TSoftObjectPtr<AActor>(TargetBindings.FindRef(Id).Get()));
		}
		for (const auto& Placement : Scene.Placements)
			for (FName Id : {Placement.PropId, Placement.MarkId})
				if (!Bindings.Targets.Contains(Id)) Bindings.Targets.Add(Id, TSoftObjectPtr<AActor>(TargetBindings.FindRef(Id).Get()));
	}
}

void AStageManager::ValidateConfiguredProduction()
{
	TArray<FString> Errors;
	ValidateProduction(ConfiguredProduction, Errors);
	for (const auto& Error : Errors)
		UE_LOG(LogStageShiftStage, Error, TEXT("%s"), *Error);
	if (Errors.IsEmpty()) UE_LOG(LogStageShiftStage, Display, TEXT("Production validation passed: %s"), *GetNameSafe(ConfiguredProduction));
}
