#include "Stage/Environment/StageControlLeverComponent.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UStageControlLeverComponent::UStageControlLeverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetMobility(EComponentMobility::Movable);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetRelativeScale3D(FVector(0.14, 0.14, 0.24));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> LeverMesh(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(
	    TEXT("/Game/LVL/Game/CrimeAndPunishment/Theatre/Materials/M_CP_OakLight"));
	if (LeverMesh.Succeeded()) SetStaticMesh(LeverMesh.Object);
	if (Wood.Succeeded()) SetMaterial(0, Wood.Object);
}

void UStageControlLeverComponent::BeginPlay()
{
	Super::BeginPlay();
	FVector Min, Max;
	GetLocalBounds(Min, Max);
	FVector Base = (Min + Max) * 0.5;
	Base.Z = Min.Z;
	PivotInMesh = Base * GetRelativeScale3D();
	RestRotation = GetRelativeRotation().Quaternion();
	PivotInParent = GetRelativeLocation() + RestRotation.RotateVector(PivotInMesh);
	if (const auto* Console = Cast<UStaticMeshComponent>(GetAttachParent()); bAutoMount && Console)
	{
		const FVector Mount = UInteractionComponent::GetControlGripLocation(*Console, MountSocket);
		PivotInParent = Console->GetComponentTransform().InverseTransformPosition(Mount);
		if (!Console->DoesSocketExist(MountSocket)) PivotInParent += MountOffset;
		SetTilt(FQuat::Identity);
	}
}

FVector UStageControlLeverComponent::GetGripLocation() const
{
	return UInteractionComponent::GetControlGripLocation(*this, GripSocket);
}

void UStageControlLeverComponent::SetTilt(const FQuat& Tilt)
{
	const FQuat Rotation = RestRotation * Tilt;
	SetRelativeLocationAndRotation(PivotInParent - Rotation.RotateVector(PivotInMesh), Rotation);
}

void UStageControlLeverComponent::PullToward(const FVector& WorldLocation)
{
	const USceneComponent* Parent = GetAttachParent();
	if (!Parent) return;
	const FVector Toward = Parent->GetComponentTransform().InverseTransformPosition(WorldLocation) - PivotInParent;
	const FVector LocalToward = RestRotation.UnrotateVector(Toward).GetSafeNormal2D();
	if (LocalToward.IsNearlyZero()) return;
	PullAxis = FVector::CrossProduct(FVector::UpVector, LocalToward);
	PullTime = 0.0f;
	SetComponentTickEnabled(true);
}

void UStageControlLeverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	PullTime += DeltaTime;
	const float Stroke = FMath::SmoothStep(0.22f, 0.45f, PullTime) * (1.0f - FMath::SmoothStep(0.50f, 0.73f, PullTime));
	SetTilt(FQuat(PullAxis, FMath::DegreesToRadians(44.0f * Stroke)));
	if (PullTime >= 0.73f) SetComponentTickEnabled(false);
}
