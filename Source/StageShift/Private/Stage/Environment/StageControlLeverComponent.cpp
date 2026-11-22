#include "Stage/Environment/StageControlLeverComponent.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Stage/Environment/StageControlMeshComponent.h"
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

void UStageControlLeverComponent::BindVisual(UStageControlMeshComponent& Mesh, FName Bone, bool bTwist)
{
	const int32 BoneIndex = Mesh.GetBoneIndex(Bone);
	if (BoneIndex == INDEX_NONE) return;
	Visual = &Mesh;
	VisualBone = Bone;
	bTwistVisual = bTwist;
	TwistAxis = Mesh.GetRefPoseTransform(BoneIndex).GetRotation().GetUpVector();
}

void UStageControlLeverComponent::PlayHandMotion(UInteractionComponent& Interaction, EInteractionHandMotion Motion) const
{
	if (const auto* Mesh = Visual.Get())
	{
		const FName Socket = Mesh->DoesSocketExist(GripSocket) ? GripSocket : VisualBone;
		Interaction.PlayHandMotion(Motion, Mesh->GetSocketLocation(Socket), Mesh, Socket);
		return;
	}
	Interaction.PlayHandMotion(Motion, UInteractionComponent::GetControlGripLocation(*this, GripSocket), this);
}

void UStageControlLeverComponent::ReleaseHandGrip(UInteractionComponent& Interaction) const
{
	const USceneComponent* Anchor = Visual.IsValid() ? static_cast<const USceneComponent*>(Visual.Get()) : this;
	Interaction.ReleaseHandGrip(*Anchor);
}

void UStageControlLeverComponent::SetTilt(const FQuat& Tilt)
{
	if (auto* Mesh = Visual.Get())
	{
		Mesh->SetControlPose(VisualBone, Tilt);
		return;
	}
	const FQuat Rotation = RestRotation * Tilt;
	SetRelativeLocationAndRotation(PivotInParent - Rotation.RotateVector(PivotInMesh), Rotation);
}

void UStageControlLeverComponent::PullToward(const FVector& WorldLocation)
{
	if (bTwistVisual && Visual.IsValid())
	{
		PullAxis = TwistAxis;
	}
	else
	{
		const USceneComponent* Parent = GetAttachParent();
		if (!Parent) return;
		const FVector Toward = Parent->GetComponentTransform().InverseTransformPosition(WorldLocation) - PivotInParent;
		const FVector LocalToward = RestRotation.UnrotateVector(Toward).GetSafeNormal2D();
		if (LocalToward.IsNearlyZero()) return;
		PullAxis = FVector::CrossProduct(FVector::UpVector, LocalToward);
	}
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
