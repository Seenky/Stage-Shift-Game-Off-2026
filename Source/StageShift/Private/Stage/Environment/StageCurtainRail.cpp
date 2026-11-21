#include "Stage/Environment/StageCurtainRail.h"

#include "Components/SplineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "Physics/PhysicsActor.h"
#include "Stage/Core/StageManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"

AStageCurtainRail::AStageCurtainRail()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Rail = CreateDefaultSubobject<USplineComponent>(TEXT("RailSpline"));
	SetRootComponent(Rail);
	Rail->ClearSplinePoints(false);
	Rail->AddSplinePoint(FVector(0, 325, 0), ESplineCoordinateSpace::Local, false);
	Rail->AddSplinePoint(FVector(0, -325, 0), ESplineCoordinateSpace::Local, false);
	Rail->SetSplinePointType(0, ESplinePointType::Linear, false);
	Rail->SetSplinePointType(1, ESplinePointType::Linear);

	Track = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Track"));
	Track->SetupAttachment(Rail);
	Track->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Track->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	if (Cube.Succeeded()) Track->SetStaticMesh(Cube.Object);
}

void AStageCurtainRail::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	const FVector Start = Rail->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::Local);
	const FVector End = Rail->GetLocationAtDistanceAlongSpline(Rail->GetSplineLength(), ESplineCoordinateSpace::Local);
	Track->SetRelativeLocation((Start + End) * .5 - FVector(0, 0, 31));
	Track->SetRelativeRotation((End - Start).Rotation());
	Track->SetRelativeScale3D(FVector(FVector::Distance(Start, End) / 100., .23, .06));
}

AActor* AStageCurtainRail::Resolve(const FSoftObjectPath& Reference) const
{
	if (Reference.IsNull() || !GetWorld()) return nullptr;
	FSoftObjectPath Path = Reference;
	if (GetWorld()->WorldType == EWorldType::PIE) Path.FixupForPIE(GetWorld()->GetPackage()->GetPIEInstanceID());
	auto* Actor = Cast<AActor>(Path.ResolveObject());
	return IsValid(Actor) && Actor->GetWorld() == GetWorld() ? Actor : nullptr;
}

void AStageCurtainRail::BeginPlay()
{
	Super::BeginPlay();
	RuntimeLever = Cast<AStagePhysicsActor>(Resolve(Lever.ToSoftObjectPath()));
	if (auto* Actor = Cast<ASkeletalMeshActor>(Resolve(Curtain.ToSoftObjectPath())))
		CurtainMesh = Actor->GetSkeletalMeshComponent();
	if (auto* Actor = RuntimeLever.Get())
	{
		Actor->SetCurtainRail(this);
		LeverRotation = Actor->GetActorRotation();
		auto* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetEnableGravity(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Actor->SetActorLocation(Rail->GetLocationAtDistanceAlongSpline(0.f, ESplineCoordinateSpace::World), false, nullptr, ETeleportType::TeleportPhysics);
	}
	ApplyCurtain();
}

void AStageCurtainRail::SetStageManager(AStageManager* Manager)
{
	StageManager = Manager;
}

bool AStageCurtainRail::CanDragLever() const
{
	return StageManager.IsValid() && StageManager->CanMoveCurtain();
}

AActor* AStageCurtainRail::GetLever() const
{
	return RuntimeLever.Get();
}

void AStageCurtainRail::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	auto* Actor = RuntimeLever.Get();
	const float Length = Rail->GetSplineLength();
	if (!Actor || Length <= KINDA_SMALL_NUMBER) return;

	const float Previous = Progress;
	const bool bDragging = CanDragLever() && Actor->IsBeingDragged();
	if (bDragging)
	{
		const float Key = Rail->FindInputKeyClosestToWorldLocation(Actor->GetActorLocation());
		const float Position = Rail->GetDistanceAlongSplineAtSplineInputKey(Key);
		Progress = FMath::Clamp(Position / Length, 0.f, 1.f);
		// Snap on arrival only, so even a small step away from an end is kept.
		if (Progress < .01f && Progress < Previous) Progress = 0.f;
		else if (Progress > .99f && Progress > Previous) Progress = 1.f;
	}
	const FVector Location = Rail->GetLocationAtDistanceAlongSpline(Progress * Length, ESplineCoordinateSpace::World);
	const FVector Tangent = Rail->GetDirectionAtDistanceAlongSpline(Progress * Length, ESplineCoordinateSpace::World);
	auto* Mesh = Actor->GetStaticMeshComponent();
	const FVector Velocity = Mesh->GetPhysicsLinearVelocity();
	const double Along = FVector::DotProduct(Velocity, Tangent);
	const bool bCanContinue = bDragging && !(Progress == 0.f && Along < 0.) && !(Progress == 1.f && Along > 0.);
	if (!Actor->GetActorLocation().Equals(Location, .25) || !Actor->GetActorRotation().Equals(LeverRotation, .1))
		Actor->SetActorLocationAndRotation(Location, LeverRotation, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetPhysicsLinearVelocity(bCanContinue ? Tangent * Along : FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	if (Previous == Progress) return;
	ApplyCurtain();
	if ((Progress == 0.f || Progress == 1.f) && StageManager.IsValid())
		StageManager->HandleCurtainEnd(IsOpen());
}

void AStageCurtainRail::ApplyCurtain() const
{
	if (auto* Mesh = CurtainMesh.Get()) Mesh->SetMorphTarget(TEXT("Open"), Progress);
}
