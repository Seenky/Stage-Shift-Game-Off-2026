#include "Stage/Environment/StageSelectionStation.h"
#include "Components/Interaction/InteractionOutlineStencil.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UI/StageSelectionWidget.h"

AStageSelectionStation::AStageSelectionStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Display = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Display"));
	SetRootComponent(Display);
	Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InteractionArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionArea"));
	InteractionArea->SetupAttachment(Display);
	InteractionArea->InitBoxExtent(FVector(28.f, 110.f, 75.f));
	InteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionArea->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PreviewA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewA"));
	PreviewA->SetupAttachment(Display);
	PreviewA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewA->SetMobility(EComponentMobility::Movable);

	PreviewB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewB"));
	PreviewB->SetupAttachment(Display);
	PreviewB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewB->SetMobility(EComponentMobility::Movable);
	PreviewB->SetVisibility(false);
}

void AStageSelectionStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PreviewA->SetStaticMesh(Options.IsEmpty() ? nullptr : Options[0].PreviewMesh.Get());
	PreviewA->SetRelativeLocation(PreviewOffset);
}

void AStageSelectionStation::BeginPlay()
{
	Super::BeginPlay();
	FInteractionOutlineStencil::MarkPlayerOcclusionBlocker(*Display);
	FInteractionOutlineStencil::MarkPlayerOcclusionBlocker(*PreviewA);
	FInteractionOutlineStencil::MarkPlayerOcclusionBlocker(*PreviewB);
	CurrentPreview = PreviewA;
	IncomingPreview = PreviewB;
}

void AStageSelectionStation::BrowseStage(int32 Direction)
{
	if (!ActiveWidget || bTravelRequested || SlideDirection != 0.f || Options.Num() < 2 || Direction == 0) return;

	const int32 Step = Direction < 0 ? -1 : 1;
	CurrentOptionIndex = (CurrentOptionIndex + Step + Options.Num()) % Options.Num();
	SlideDirection = Step;
	SlideElapsed = 0.f;
	IncomingPreview->SetStaticMesh(Options[CurrentOptionIndex].PreviewMesh);
	IncomingPreview->SetRelativeLocation(PreviewOffset + FVector(0, SlideDirection * RailTravelDistance, 0));
	IncomingPreview->SetVisibility(true);
	if (SlideSound) UGameplayStatics::PlaySoundAtLocation(this, SlideSound, GetActorLocation());
	RefreshSelectionWidget();
	SetActorTickEnabled(true);
}

void AStageSelectionStation::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	SlideElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(SlideElapsed / FMath::Max(RailTravelTime, .01f), 0.f, 1.f);
	const float Distance = SlideDirection * RailTravelDistance;
	const float Progress = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
	CurrentPreview->SetRelativeLocation(PreviewOffset + FVector(0, -Distance * Progress, 0));
	IncomingPreview->SetRelativeLocation(PreviewOffset + FVector(0, Distance * (1.f - Progress), 0));
	if (Alpha >= 1.f)
	{
		CurrentPreview->SetVisibility(false);
		Swap(CurrentPreview, IncomingPreview);
		SlideDirection = 0.f;
		SetActorTickEnabled(false);
		RefreshSelectionWidget();
	}
}

const UMeshComponent* AStageSelectionStation::GetInteractionMesh() const
{
	return Display;
}

bool AStageSelectionStation::CanInteract_Implementation(const FInteractionContext& Context) const
{
	const APawn* Pawn = Cast<APawn>(Context.Interactor);
	const APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return Player && Player->IsLocalController() && !ActiveWidget && !bTravelRequested && SelectionCamera && WidgetClass && !Options.IsEmpty();
}

EInteractionResult AStageSelectionStation::OnInteraction_Implementation(const FInteractionContext& Context)
{
	if (Context.InteractionPhase != EInteractionPhase::Start || !CanInteract_Implementation(Context))
	{
		return EInteractionResult::Rejected;
	}

	APlayerController* Player = CastChecked<APlayerController>(CastChecked<APawn>(Context.Interactor)->GetController());
	ActiveWidget = CreateWidget<UStageSelectionWidget>(Player, WidgetClass);
	if (!ActiveWidget) return EInteractionResult::Rejected;

	ActiveWidget->SetStation(this);
	RefreshSelectionWidget();
	if (!ActiveWidget->AddToPlayerScreen())
	{
		ActiveWidget = nullptr;
		return EInteractionResult::Rejected;
	}

	PreviousCamera = Player->GetViewTarget();
	bWasCursorVisible = Player->bShowMouseCursor;
	if (!Player->HiddenActors.Contains(Context.Interactor))
	{
		Player->HiddenActors.Add(Context.Interactor);
		HiddenInteractor = Context.Interactor.Get();
	}
	Player->SetViewTargetWithBlend(SelectionCamera, CameraBlendTime, VTBlend_Cubic);
	Player->bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(Player, ActiveWidget, EMouseLockMode::DoNotLock, true);
	ActiveWidget->FocusSelection();
	if (OpenSound) UGameplayStatics::PlaySound2D(this, OpenSound);
	return EInteractionResult::Completed;
}

void AStageSelectionStation::RefreshSelectionWidget()
{
	if (ActiveWidget)
	{
		ActiveWidget->ShowOption(Options[CurrentOptionIndex], Options.Num() > 1, SlideDirection != 0.f);
	}
}

bool AStageSelectionStation::TryStartStage()
{
	if (!ActiveWidget || bTravelRequested || SlideDirection != 0.f || !Options[CurrentOptionIndex].CanStart()) return false;

	const TSoftObjectPtr<UWorld>& Level = Options[CurrentOptionIndex].Level;
	if (!FPackageName::DoesPackageExist(Level.ToSoftObjectPath().GetLongPackageName())) return false;

	bTravelRequested = true;
	if (ConfirmSound) UGameplayStatics::SpawnSound2D(this, ConfirmSound, 1.f, 1.f, 0.f, nullptr, true);
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Level);
	return true;
}

void AStageSelectionStation::CloseSelection()
{
	if (!ActiveWidget || bTravelRequested) return;

	if (BackSound) UGameplayStatics::PlaySound2D(this, BackSound);
	ReleaseSelection(true);
}

void AStageSelectionStation::ReleaseSelection(bool bRestoreView)
{
	if (!ActiveWidget) return;

	if (APlayerController* Player = ActiveWidget->GetOwningPlayer())
	{
		if (AActor* Interactor = HiddenInteractor.Get()) Player->HiddenActors.Remove(Interactor);
		if (bRestoreView)
		{
			if (AActor* Camera = PreviousCamera.Get())
			{
				Player->SetViewTargetWithBlend(Camera, CameraBlendTime, VTBlend_Cubic);
			}
			Player->bShowMouseCursor = bWasCursorVisible;
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(Player, true);
		}
	}
	ActiveWidget->RemoveFromParent();
	ActiveWidget = nullptr;
	PreviousCamera.Reset();
	HiddenInteractor.Reset();
}

void AStageSelectionStation::EndPlay(EEndPlayReason::Type Reason)
{
	ReleaseSelection(Reason == EEndPlayReason::Destroyed && !bTravelRequested);
	Super::EndPlay(Reason);
}
