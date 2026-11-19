#include "Stage/Cues/StageCueWorldMarker.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

AStageCueWorldMarker::AStageCueWorldMarker()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MarkerRoot"));
	RootComponent->SetUsingAbsoluteRotation(true);
	RootComponent->SetUsingAbsoluteScale(true);
}

bool AStageCueWorldMarker::HasValidTarget() const
{
	return TrackedActor.IsValid() && !TrackedActor->IsActorBeingDestroyed();
}

void AStageCueWorldMarker::Configure(AActor* Target, const FStageCueTarget& Definition, const FStageCueView& Cue)
{
	if (!IsValid(Target) || !Target->GetRootComponent()) return;
	if (TrackedActor != Target || MarkerType != Definition.Type)
	{
		ReleaseHighlight();
		TrackedActor = Target;
		AttachToComponent(Target->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		if (Definition.Type == EStageCueMarkerType::Object && ObjectMaterial)
		{
			ObjectInstance = UMaterialInstanceDynamic::Create(ObjectMaterial, this);
			TInlineComponentArray<UMeshComponent*> Meshes(Target);
			for (UMeshComponent* Mesh : Meshes)
			{
				// Respect authored overlays and other systems; the numbered marker still remains.
				if (!IsValid(Mesh) || Mesh->GetOverlayMaterial()) continue;
				Mesh->SetOverlayMaterial(ObjectInstance);
				HighlightedMeshes.Add(Mesh);
			}
		}
	}
	CueView = Cue;
	MarkerType = Definition.Type;
	TargetOffset = Definition.Offset;
	Tint = Cue.State == EStageCueState::Problem ? ProblemColor : Cue.State == EStageCueState::Upcoming ? UpcomingColor : TaskColor;
	SetActorRelativeLocation(TargetOffset);
	SetActorRotation(FRotator::ZeroRotator);
	if (ObjectInstance) ObjectInstance->SetVectorParameterValue(TEXT("CueColor"), Tint);
	const bool bShowZone = MarkerType == EStageCueMarkerType::Zone && ZoneMesh && ZoneMaterial;
	if (bShowZone)
	{
		if (!ZoneVisual)
		{
			ZoneVisual = NewObject<UStaticMeshComponent>(this);
			ZoneVisual->SetupAttachment(RootComponent);
			ZoneVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ZoneVisual->SetCanEverAffectNavigation(false);
			ZoneVisual->SetCastShadow(false);
			ZoneVisual->SetRelativeLocation(FVector(0.0, 0.0, 2.0));
			ZoneVisual->SetStaticMesh(ZoneMesh);
			ZoneInstance = UMaterialInstanceDynamic::Create(ZoneMaterial, this);
			ZoneVisual->SetMaterial(0, ZoneInstance);
			ZoneVisual->RegisterComponent();
		}
		ZoneInstance->SetVectorParameterValue(TEXT("CueColor"), Tint);
		const float Radius = FMath::Clamp(Definition.Radius, 10.0f, 500.0f);
		const FVector Extent = ZoneMesh->GetBounds().BoxExtent;
		ZoneVisual->SetRelativeScale3D(FVector(Radius / FMath::Max(Extent.X, 1.0), Radius / FMath::Max(Extent.Y, 1.0), 1.0));
	}
	if (ZoneVisual) ZoneVisual->SetVisibility(bShowZone);
}

FVector AStageCueWorldMarker::GetIndicatorLocation() const
{
	if (!HasValidTarget()) return GetActorLocation();
	if (MarkerType == EStageCueMarkerType::Zone) return GetActorLocation() + FVector(0.0, 0.0, 20.0);
	FVector Origin, Extent;
	TrackedActor->GetActorBounds(true, Origin, Extent);
	// Dialogue owns the space above actors. Actor cues sit beside their feet.
	if (MarkerType == EStageCueMarkerType::Recipient || MarkerType == EStageCueMarkerType::Event)
		return Origin + TargetOffset + FVector(0.0, 0.0, -Extent.Z + 25.0);
	return Origin + TargetOffset + FVector(0.0, 0.0, Extent.Z + 22.0);
}

void AStageCueWorldMarker::ReleaseHighlight()
{
	for (const auto& WeakMesh : HighlightedMeshes)
		if (UMeshComponent* Mesh = WeakMesh.Get(); Mesh && Mesh->GetOverlayMaterial() == ObjectInstance) Mesh->SetOverlayMaterial(nullptr);
	HighlightedMeshes.Reset();
	ObjectInstance = nullptr;
}

void AStageCueWorldMarker::EndPlay(const EEndPlayReason::Type Reason)
{
	ReleaseHighlight();
	Super::EndPlay(Reason);
}
