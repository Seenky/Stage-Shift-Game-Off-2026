#include "Stage/Environment/StageTurntable.h"

#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Physics/PhysicsActor.h"
#include "Stage/Core/StageManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"

AStageTurntable::AStageTurntable()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	USceneComponent* Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	SetRootComponent(Pivot);
	Pivot->SetMobility(EComponentMobility::Movable);

	Platform = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Platform"));
	Platform->SetupAttachment(Pivot);
	Platform->SetMobility(EComponentMobility::Movable);
	Platform->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Platform->CanCharacterStepUpOn = ECB_Yes;
	Platform->SetRelativeLocation(FVector(0.0f, 0.0f, -14.0f));
	Platform->SetRelativeScale3D(FVector(13.0f, 13.0f, 0.28f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	if (Cylinder.Succeeded()) Platform->SetStaticMesh(Cylinder.Object);
}

void AStageTurntable::BeginPlay()
{
	Super::BeginPlay();
	InitialRotation = GetActorRotation();
	AttachScenery();
}

void AStageTurntable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAudioComponent* Audio = RotationAudio.Get()) Audio->Stop();
	if (EndPlayReason == EEndPlayReason::Destroyed || EndPlayReason == EEndPlayReason::RemovedFromWorld) ReleaseProps();
	Super::EndPlay(EndPlayReason);
}

void AStageTurntable::AttachScenery()
{
	const UWorld* World = GetWorld();
	for (const TSoftObjectPtr<AActor>& Reference : SceneryToRotate)
	{
		FSoftObjectPath Path = Reference.ToSoftObjectPath();
		if (World->WorldType == EWorldType::PIE) Path.FixupForPIE(World->GetPackage()->GetPIEInstanceID());
		AActor* Scenery = Cast<AActor>(Path.ResolveObject());
		if (IsValid(Scenery) && Scenery != this && Scenery->GetWorld() == World && Scenery->GetAttachParentActor() != this)
			Scenery->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void AStageTurntable::SecureProps()
{
	constexpr double SupportGap = 12.0;
	TMap<AStagePhysicsActor*, AActor*> Supports;
	for (TActorIterator<AStagePhysicsActor> It(GetWorld()); It; ++It)
	{
		AStagePhysicsActor* Prop = *It;
		const auto* Mesh = Prop->GetStaticMeshComponent();
		if (!Mesh->IsSimulatingPhysics() || Prop->GetAttachParentActor() || Prop->IsCurtainLever() || Prop->IsBeingManipulated()) continue;

		const FVector Start = Mesh->Bounds.Origin;
		const FVector End = Start - FVector(0.0, 0.0, Mesh->Bounds.BoxExtent.Z + SupportGap);
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(TurntablePropSupport), false, Prop);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Mesh->GetCollisionObjectType(), Query))
			if (const auto* Surface = Hit.GetComponent(); Surface && Surface->IsPhysicsCollisionEnabled() && Hit.ImpactNormal.Z > 0.5)
				Supports.Add(Prop, Hit.GetActor());
	}

	// Trace before changing physics, then secure the bottom of each stack first.
	bool bSecured;
	do
	{
		bSecured = false;
		for (auto It = Supports.CreateIterator(); It; ++It)
		{
			const AActor* Support = It.Value();
			if (!Support || (Support != this && !Support->IsAttachedTo(this))) continue;

			AStagePhysicsActor* Prop = It.Key();
			auto* Mesh = Prop->GetStaticMeshComponent();
			Mesh->SetSimulatePhysics(false);
			if (Prop->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform))
			{
				SecuredProps.Add({Prop, Mesh->GetCollisionEnabled()});
				Mesh->SetCollisionEnabled(Mesh->IsQueryCollisionEnabled() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
				bSecured = true;
			}
			else Mesh->SetSimulatePhysics(true);
			It.RemoveCurrent();
		}
	} while (bSecured && !Supports.IsEmpty());
}

void AStageTurntable::ReleaseProps()
{
	for (const FSecuredProp& Entry : SecuredProps)
	{
		auto* Prop = Entry.Prop.Get();
		if (!IsValid(Prop) || Prop->GetAttachParentActor() != this) continue;
		Prop->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		auto* Mesh = Prop->GetStaticMeshComponent();
		Mesh->SetCollisionEnabled(Entry.Collision);
		Mesh->SetSimulatePhysics(true);
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	}
	SecuredProps.Reset();
}

void AStageTurntable::SetStageManager(AStageManager* Manager)
{
	StageManager = Manager;
}

bool AStageTurntable::CanRotate() const
{
	const AStageManager* Manager = StageManager.Get();
	return !bRotating && (!Manager || (Manager->GetState() == EStageState::Idle && Manager->CanUseTurntable()));
}

bool AStageTurntable::IsNavigationReady() const
{
	UWorld* World = GetWorld();
	return World && !bRotating && !UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World);
}

bool AStageTurntable::RotateNext()
{
	return RotateToSector((CurrentSector + 1) % FMath::Max(2, SectorCount));
}

bool AStageTurntable::RotateToSector(int32 SectorIndex)
{
	if (!HasActorBegunPlay() || !CanRotate() || SectorIndex < 0 || SectorIndex >= FMath::Max(2, SectorCount) || SectorIndex == CurrentSector) return false;
	TargetSector = SectorIndex;
	StartYaw = GetActorRotation().Yaw;
	const float SectorYaw = InitialRotation.Yaw + 360.0f * static_cast<float>(SectorIndex) / FMath::Max(2, SectorCount);
	EndYaw = StartYaw + FMath::FindDeltaAngleDegrees(StartYaw, SectorYaw);
	RotationElapsed = 0.0f;
	bRotating = true;
	AttachScenery();
	SecureProps();
	if (AStageManager* Manager = StageManager.Get()) Manager->NotifyTurntableRotationStarted();
	if (RotationSound) UGameplayStatics::PlaySoundAtLocation(this, RotationSound, GetActorLocation());
	if (RotationLoopSound)
	{
		RotationAudio = UGameplayStatics::SpawnSoundAttached(RotationLoopSound, GetRootComponent(), NAME_None,
			FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true);
	}
	SetActorTickEnabled(true);
	return true;
}

void AStageTurntable::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bRotating) return;

	RotationElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(RotationElapsed / FMath::Max(0.1f, RotationDuration), 0.0f, 1.0f);
	const float Blend = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	SetActorRotation(FRotator(InitialRotation.Pitch, FMath::Lerp(StartYaw, EndYaw, Blend), InitialRotation.Roll));
	if (Alpha < 1.0f) return;

	CurrentSector = TargetSector;
	bRotating = false;
	SetActorTickEnabled(false);
	if (UAudioComponent* Audio = RotationAudio.Get()) Audio->FadeOut(0.18f, 0.0f);
	RotationAudio.Reset();
	if (RotationStopSound) UGameplayStatics::PlaySoundAtLocation(this, RotationStopSound, GetActorLocation());
	ReleaseProps();
	OnArrived.Broadcast(CurrentSector);
}
