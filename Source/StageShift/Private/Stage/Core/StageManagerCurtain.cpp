#include "Stage/Core/StageManager.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/Physics/DollPhysicsComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Core/StageShiftCharacterMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "Sound/SoundBase.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Cues/StageCueComponent.h"
#include "Stage/Environment/StageCurtainRail.h"
#include "Stage/Environment/StageTurntable.h"
#include "Stage/Environment/StageTurntableControl.h"
#include "UObject/Package.h"

namespace
{
	const FName CurtainSignal(TEXT("Curtain"));
	const FName TurntableSignal(TEXT("Turntable"));
	constexpr float RehearsalSeconds = 30.f;
	constexpr float CastWalkTimeout = 8.f;
	constexpr float MarkRadius = 55.f;

	FVector CastLocation(const AActor& Mark, const AActor* Performer)
	{
		const auto* Character = Cast<ACharacter>(Performer);
		const double Height = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.;
		return Mark.GetActorLocation() + FVector(0, 0, Height);
	}

	FRotator CastRotation(const AActor& Mark, const AActor* Performer)
	{
		const FRotator Rotation = Mark.GetActorRotation();
		return Cast<ACharacter>(Performer) ? FRotator(0.0, Rotation.Yaw, 0.0) : Rotation;
	}
}

AActor* AStageManager::ResolveLevelActor(const FSoftObjectPath& Reference) const
{
	const UWorld* World = GetWorld();
	if (!World || Reference.IsNull()) return nullptr;
	FSoftObjectPath Path = Reference;
	if (World->WorldType == EWorldType::PIE) Path.FixupForPIE(World->GetPackage()->GetPIEInstanceID());
	auto* Actor = Cast<AActor>(Path.ResolveObject());
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == World ? Actor : nullptr;
}

bool AStageManager::IsCurtainOpeningOverdue() const
{
	return CurtainPhase == ECurtainPhase::Opening && NextOpeningPenaltyAt >= 0.0;
}

bool AStageManager::CanMoveCurtain() const
{
	return ActiveProduction && (CurtainPhase == ECurtainPhase::Opening ||
	    (CurtainPhase == ECurtainPhase::Closing && State == EStageState::Idle));
}

bool AStageManager::CanUseTurntable() const
{
	if (!RuntimeCurtain.IsValid()) return true;
	const AStageTurntable* Platform = ResolveTurntable();
	return ActiveProduction && CurtainPhase == ECurtainPhase::Rotating && Platform && Platform->GetCurrentSector() != ExpectedSector;
}

void AStageManager::BeginCurtainOpening(bool bFirstScene)
{
	CurtainPhase = ECurtainPhase::Opening;
	OpeningDeadline = bFirstScene ? GetWorld()->GetTimeSeconds() + RehearsalSeconds : -1.;
	NextOpeningPenaltyAt = -1.;
	SetCurtainSignal(true);
}

void AStageManager::SetCurtainSignal(bool bVisible)
{
	if (!bVisible) PlayerCues->DismissCue(CurtainSignal);
	else if (const auto* Curtain = RuntimeCurtain.Get())
		PlayerCues->ShowActorSignal(CurtainSignal, EStageCueKind::Mechanism, Curtain->GetLever());
}

void AStageManager::SetTurntableSignal(bool bVisible)
{
	if (!bVisible)
	{
		PlayerCues->DismissCue(TurntableSignal);
		return;
	}
	const AStageTurntable* Platform = ResolveTurntable();
	if (!Platform) return;
	for (TActorIterator<AStageTurntableControl> Control(GetWorld()); Control; ++Control)
		if (Control->Turntable == Platform)
		{
			PlayerCues->ShowActorSignal(TurntableSignal, EStageCueKind::Mechanism, *Control);
			break;
		}
}

void AStageManager::NotifyTurntableRotationStarted()
{
	if (CurtainPhase == ECurtainPhase::Rotating) SetTurntableSignal(false);
}

void AStageManager::HandleCurtainEnd(bool bOpen)
{
	if (!ActiveProduction || !ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex)) return;
	if (bOpen && CurtainPhase == ECurtainPhase::Opening)
	{
		OpeningDeadline = NextOpeningPenaltyAt = -1.;
		SetCurtainSignal(false);
	}
	else if (!bOpen && CurtainPhase == ECurtainPhase::Closing)
	{
		SetCurtainSignal(false);
		MoveCurtainCast(ActiveProduction->Scenes[ProductionSceneIndex], false);
	}
}

bool AStageManager::PrepareCurtainCast(const FStageProductionScene& Scene, FText& Error)
{
	const auto* Bindings = ProductionBindings.Find(Scene.SceneId);
	if (!Bindings) return true;
	for (const auto& [RoleId, Placement] : Bindings->Cast)
	{
		if (!Placement.ActorClass) continue;
		const AActor* Backstage = ResolveLevelActor(Placement.BackstageMark.ToSoftObjectPath());
		if (!Backstage)
		{
			Error = FText::FromString(TEXT("Missing backstage mark for ") + RoleId.ToString());
			return false;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const AActor* DefaultPerformer = Placement.ActorClass->GetDefaultObject<AActor>();
		AActor* Performer = GetWorld()->SpawnActor<AActor>(Placement.ActorClass,
		    CastLocation(*Backstage, DefaultPerformer), CastRotation(*Backstage, DefaultPerformer), Params);
		if (!Performer || !Performer->FindComponentByClass<UStageMemberComponent>())
		{
			if (Performer) Performer->Destroy();
			Error = FText::FromString(TEXT("Could not spawn stage member ") + RoleId.ToString());
			return false;
		}
		if (auto* Pawn = Cast<APawn>(Performer); Pawn && !Pawn->GetController()) Pawn->SpawnDefaultController();
		RuntimeCast.Add(RoleId, Performer);
	}
	return true;
}

void AStageManager::MoveCurtainCast(const FStageProductionScene& Scene, bool bEntering)
{
	CurtainPhase = bEntering ? ECurtainPhase::Entering : ECurtainPhase::Exiting;
	CastMoveDeadline = GetWorld()->GetTimeSeconds() + CastWalkTimeout;
	ClearCurtainCastMoves();
	const auto Actual = ResolveProductionBindings(Scene);
	const auto* Bindings = ProductionBindings.Find(Scene.SceneId);
	for (FName RoleId : Scene.Scenario->GetRoleOptions())
	{
		auto* Actor = Actual.Roles.FindRef(RoleId).Get();
		if (!IsValid(Actor)) continue;
		const auto* Placement = Bindings ? Bindings->Cast.Find(RoleId) : nullptr;
		auto* Mark = Placement ? ResolveLevelActor((bEntering ? Placement->StageMark : Placement->BackstageMark).ToSoftObjectPath()) : nullptr;
		CastMoves.Add({Actor, Mark});
		if (!bEntering)
		{
			if (auto* Physics = Actor->FindComponentByClass<UPhysicalMontageComponent>()) Physics->EndStageFall();
			if (auto* Interaction = Actor->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
			if (auto* Manipulation = Actor->FindComponentByClass<UPhysicsManipulationComponent>()) Manipulation->EndManipulation(0.f, true);
		}
	}

	for (FCastMove& Move : CastMoves)
	{
		const auto* Character = Cast<ACharacter>(Move.Actor.Get());
		if (!Character) continue;
		// Match the navigation agent's capsule while walking through the scenery.
		if (auto* Movement = Cast<UStageShiftCharacterMovementComponent>(Character->GetCharacterMovement()))
			Movement->SetStageTransitionMovement(true);
		auto* Capsule = Character->GetCapsuleComponent();
		for (const FCastMove& Other : CastMoves)
		{
			AActor* OtherActor = Other.Actor.Get();
			if (!OtherActor || OtherActor == Character || Capsule->GetMoveIgnoreActors().Contains(OtherActor)) continue;
			Capsule->IgnoreActorWhenMoving(OtherActor, true);
			Move.AddedMoveIgnores.Add(OtherActor);
		}
		// Capsule ignores do not affect the simulated bodies that drive the doll mesh.
		if (auto* Doll = Character->FindComponentByClass<UDollPhysicsComponent>(); Doll && Doll->bEnabled)
		{
			Move.bRestoreDollPhysics = true;
			Doll->bEnabled = false;
		}
	}
}

void AStageManager::ClearCurtainCastMoves()
{
	for (const FCastMove& Move : CastMoves)
	{
		const auto* Character = Cast<ACharacter>(Move.Actor.Get());
		if (!Character) continue;
		if (auto* Controller = Character->GetController<AAIController>()) Controller->StopMovement();
		if (auto* Movement = Cast<UStageShiftCharacterMovementComponent>(Character->GetCharacterMovement()))
			Movement->SetStageTransitionMovement(false);
		for (const auto& Ignored : Move.AddedMoveIgnores)
			if (AActor* Actor = Ignored.Get()) Character->GetCapsuleComponent()->IgnoreActorWhenMoving(Actor, false);
		if (Move.bRestoreDollPhysics)
			if (auto* Doll = Character->FindComponentByClass<UDollPhysicsComponent>()) Doll->bEnabled = true;
	}
	CastMoves.Reset();
}

void AStageManager::FinishCurtainCastMoves(bool bEntering)
{
	for (const auto& Move : CastMoves)
	{
		AActor* Actor = Move.Actor.Get();
		if (!Actor) continue;
		const auto* Pawn = Cast<APawn>(Actor);
		if (auto* Controller = Pawn ? Pawn->GetController<AAIController>() : nullptr) Controller->StopMovement();
		if (bEntering)
		{
			if (const AActor* Mark = Move.Mark.Get())
			{
				if (FVector::DistSquared2D(Actor->GetActorLocation(), Mark->GetActorLocation()) > FMath::Square(MarkRadius))
					Actor->SetActorLocation(CastLocation(*Mark, Actor), false, nullptr, ETeleportType::TeleportPhysics);
				Actor->SetActorRotation(CastRotation(*Mark, Actor));
			}
		}
		else Actor->Destroy();
	}
	ClearCurtainCastMoves();
	if (bEntering)
	{
		const UStageDataAsset* Scenario = ActiveProduction->Scenes[ProductionSceneIndex].Scenario;
		if (Scenario && !Scenario->SceneBriefing.IsEmpty())
		{
			CurtainPhase = ECurtainPhase::Briefing;
			FText Error;
			if (!QueueSceneBriefing(Scenario->SceneBriefing, Error)) FinishProduction(false, Error);
		}
		else BeginCurtainOpening(false);
		return;
	}

	RuntimeCast.Reset();
	++ProductionSceneIndex;
	if (!ActiveProduction->Scenes.IsValidIndex(ProductionSceneIndex))
	{
		FinishProduction(true, FText::GetEmpty());
		return;
	}
	const auto* Platform = ResolveTurntable();
	if (!Platform)
	{
		FinishProduction(false, NSLOCTEXT("StageProduction", "MissingTurntable", "Для смены сцены нужна поворотная платформа."));
		return;
	}
	ExpectedSector = (Platform->GetCurrentSector() + 1) % FMath::Max(2, Platform->SectorCount);
	CurtainPhase = ECurtainPhase::Rotating;
	AdvanceProduction();
	if (CanUseTurntable()) SetTurntableSignal(true);
}

void AStageManager::TickCurtainProduction()
{
	auto* Curtain = RuntimeCurtain.Get();
	const double Now = GetWorld()->GetTimeSeconds();
	switch (CurtainPhase)
	{
	case ECurtainPhase::Opening:
		if (Curtain->IsOpen())
		{
			if (GetPreparationErrors().IsEmpty())
			{
				FText Error;
				ConfirmProductionPreparation(Error);
			}
			break;
		}
		if (OpeningDeadline >= 0. && Now >= OpeningDeadline)
		{
			if (NextOpeningPenaltyAt < 0.)
			{
				NextOpeningPenaltyAt = OpeningDeadline;
				if (OpeningOverdueSound) UGameplayStatics::PlaySound2D(this, OpeningOverdueSound);
				OnOpeningOverdue.Broadcast();
			}
			if (Now >= NextOpeningPenaltyAt)
			{
				const int32 Seconds = FMath::FloorToInt(Now - NextOpeningPenaltyAt) + 1;
				NextOpeningPenaltyAt += Seconds;
				AddScore(-Seconds);
			}
		}
		break;
	case ECurtainPhase::Rotating:
		if (const auto* Platform = ResolveTurntable(); Platform && Platform->GetCurrentSector() == ExpectedSector && Platform->IsNavigationReady())
			MoveCurtainCast(ActiveProduction->Scenes[ProductionSceneIndex], true);
		break;
	case ECurtainPhase::Exiting:
	case ECurtainPhase::Entering:
		for (FCastMove& Move : CastMoves)
		{
			AActor* Actor = Move.Actor.Get();
			const AActor* Mark = Move.Mark.Get();
			if (Move.bStarted || !Actor || !Mark) continue;
			const auto* Physics = Actor->FindComponentByClass<UPhysicalMontageComponent>();
			if (Physics && Physics->IsPhysicsMontageActive()) continue;
			const auto* Pawn = Cast<APawn>(Actor);
			if (auto* Controller = Pawn ? Pawn->GetController<AAIController>() : nullptr)
				Controller->MoveToLocation(Mark->GetActorLocation(), MarkRadius, false, true, true, false, nullptr, false);
			Move.bStarted = true;
		}
		if (Now >= CastMoveDeadline || !CastMoves.ContainsByPredicate([](const FCastMove& Move)
		{
			const AActor* Actor = Move.Actor.Get();
			const AActor* Mark = Move.Mark.Get();
			return Actor && Mark && FVector::DistSquared2D(Actor->GetActorLocation(), Mark->GetActorLocation()) > FMath::Square(MarkRadius);
		}))
			FinishCurtainCastMoves(CurtainPhase == ECurtainPhase::Entering);
		break;
	default:
		break;
	}
}
