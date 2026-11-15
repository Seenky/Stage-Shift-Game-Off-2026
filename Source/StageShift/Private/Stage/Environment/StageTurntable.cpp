#include "Stage/Environment/StageTurntable.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

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
}

bool AStageTurntable::RotateNext()
{
	return RotateToSector((CurrentSector + 1) % FMath::Max(2, SectorCount));
}

bool AStageTurntable::RotateToSector(int32 SectorIndex)
{
	if (!HasActorBegunPlay() || bRotating || SectorIndex < 0 || SectorIndex >= FMath::Max(2, SectorCount) || SectorIndex == CurrentSector) return false;
	TargetSector = SectorIndex;
	StartYaw = GetActorRotation().Yaw;
	const float SectorYaw = InitialRotation.Yaw + 360.0f * static_cast<float>(SectorIndex) / FMath::Max(2, SectorCount);
	EndYaw = StartYaw + FMath::FindDeltaAngleDegrees(StartYaw, SectorYaw);
	RotationElapsed = 0.0f;
	bRotating = true;
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
	OnArrived.Broadcast(CurrentSector);
}
