#include "Stage/Environment/StageBackstageShortcut.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "UI/StageCueWidget.h"

AStageBackstageShortcut::AStageBackstageShortcut()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Approach = CreateDefaultSubobject<UBoxComponent>(TEXT("Approach"));
	Approach->SetupAttachment(GetRootComponent());
	Approach->SetRelativeLocation(FVector(143, 0, 80));
	Approach->InitBoxExtent(FVector(65, 100, 85));
	Approach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Approach->SetCollisionResponseToAllChannels(ECR_Ignore);
	Approach->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Approach->SetCanEverAffectNavigation(false);

	Entrance = CreateDefaultSubobject<UBoxComponent>(TEXT("DiveEntrance"));
	Entrance->SetupAttachment(GetRootComponent());
	Entrance->SetRelativeLocation(FVector(100, 0, 150));
	Entrance->InitBoxExtent(FVector(14, 133, 140));
	Entrance->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Entrance->SetCanEverAffectNavigation(false);

	Exit = CreateDefaultSubobject<USceneComponent>(TEXT("Exit"));
	Exit->SetupAttachment(GetRootComponent());
	Exit->SetRelativeLocation(FVector(132, 0, 0));

	Signal = CreateDefaultSubobject<UWidgetComponent>(TEXT("Signal"));
	Signal->SetupAttachment(GetRootComponent());
	Signal->SetRelativeLocation(FVector(0, 0, 300));
	Signal->SetWidgetSpace(EWidgetSpace::Screen);
	Signal->SetDrawSize(FVector2D(80, 80));
	Signal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Signal->SetVisibility(false);
	SignalWidgetClass = TSoftClassPtr<UStageCueSignalWidget>(FSoftObjectPath(TEXT("/Game/DEV/UI/StageSignals/W_StageCueSignal.W_StageCueSignal_C")));
}

void AStageBackstageShortcut::BeginPlay()
{
	Super::BeginPlay();
	Signal->SetWidgetClass(SignalWidgetClass.LoadSynchronous());
}

void AStageBackstageShortcut::EndPlay(EEndPlayReason::Type Reason)
{
	SetNearby(false);
	HighlightScreen(false);
	Super::EndPlay(Reason);
}

AStageBackstageShortcut* AStageBackstageShortcut::GetPartner() const
{
	return IsValid(Partner) && Partner != this && !Partner->IsActorBeingDestroyed() && Partner->Partner == this ? Partner.Get() : nullptr;
}

FTransform AStageBackstageShortcut::GetEntranceTransform() const
{
	return Entrance->GetComponentTransform();
}

FVector AStageBackstageShortcut::GetExitFloorLocation() const
{
	return Exit->GetComponentLocation();
}

bool AStageBackstageShortcut::IsDiveEntering(const FVector& Previous, const FVector& Current) const
{
	const FTransform& Plane = Entrance->GetComponentTransform();
	const FVector Start = Plane.InverseTransformPosition(Previous);
	const FVector End = Plane.InverseTransformPosition(Current);
	const FVector Delta = End - Start;
	const FVector Extent = Entrance->GetUnscaledBoxExtent();
	return Delta.X < -UE_KINDA_SMALL_NUMBER && FMath::LineBoxIntersection(FBox(-Extent, Extent), Start, End, Delta);
}

void AStageBackstageShortcut::SetNearby(bool bNearby)
{
	UpdateCanvasSpeed(bNearby);
	ShowSignal(bNearby, true);
	if (auto* Destination = GetPartner())
	{
		Destination->ShowSignal(bNearby, false);
		Destination->HighlightScreen(bNearby);
	}
}

void AStageBackstageShortcut::UpdateCanvasSpeed(bool bNearby)
{
	if (!CanvasParameters || !GetWorld()) return;
	auto* Parameters = GetWorld()->GetParameterCollectionInstance(CanvasParameters);
	static const FName SpeedName(TEXT("FlowSpeed"));
	static const FName OffsetName(TEXT("FlowOffset"));
	float PreviousSpeed, Offset;
	const float NewSpeed = bNearby ? 0.75f : 0.18f;
	if (!Parameters || !Parameters->GetScalarParameterValue(SpeedName, PreviousSpeed) || PreviousSpeed == NewSpeed
	    || !Parameters->GetScalarParameterValue(OffsetName, Offset)) return;

	// Keep Time * Speed + Offset continuous when the hints appear or disappear.
	const double AdjustedOffset = Offset + GetWorld()->GetTimeSeconds() * (PreviousSpeed - NewSpeed);
	Parameters->SetScalarParameterValue(OffsetName, FMath::Frac(AdjustedOffset));
	Parameters->SetScalarParameterValue(SpeedName, NewSpeed);
}

void AStageBackstageShortcut::ShowSignal(bool bShow, bool bEntry)
{
	UTexture2D* Icon = bEntry ? EntryIcon.Get() : ExitIcon.Get();
	Signal->SetVisibility(bShow && Icon);
	if (!bShow || !Icon) return;
	Signal->InitWidget();
	if (auto* Widget = Cast<UStageCueSignalWidget>(Signal->GetUserWidgetObject()))
	{
		const bool bMirrored = bEntry ? bMirrorEntryIcon : !bMirrorEntryIcon;
		Widget->SetRenderScale(FVector2D(bMirrored ? -1.0 : 1.0, 1.0));
		Widget->UpdateSignal(Icon, 1.f, false, 0.f);
	}
}

void AStageBackstageShortcut::HighlightScreen(bool bHighlight)
{
	if (!IsValid(Screen) || !HighlightMaterial) return;
	auto* Mesh = Screen->GetStaticMeshComponent();
	if (bHighlight && !Mesh->GetOverlayMaterial())
	{
		if (!HighlightInstance)
		{
			HighlightInstance = UMaterialInstanceDynamic::Create(HighlightMaterial, this);
			HighlightInstance->SetVectorParameterValue(TEXT("CueColor"), FLinearColor(2.4f, 1.55f, 0.5f));
		}
		Mesh->SetOverlayMaterial(HighlightInstance);
	}
	else if (!bHighlight && HighlightInstance && Mesh->GetOverlayMaterial() == HighlightInstance)
	{
		Mesh->SetOverlayMaterial(nullptr);
	}
}
