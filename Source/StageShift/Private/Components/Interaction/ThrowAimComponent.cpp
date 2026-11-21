#include "Components/Interaction/ThrowAimComponent.h"

#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "SceneView.h"
#include "UI/ThrowAimWidget.h"
#include "UObject/ConstructorHelpers.h"

#if ENABLE_DRAW_DEBUG
namespace
{
TAutoConsoleVariable<int32> CVarDebugThrowTrace(TEXT("Stage.DebugThrowTrace"), 0,
    TEXT("Draw the cursor ray and the box sweeps used for the throw landing preview: 1 = on, 0 = off."), ECVF_Cheat);
}
#endif

UThrowAimComponent::UThrowAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	ReticleWidgetClass = UThrowAimWidget::StaticClass();
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Dot(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DotMaterial(TEXT("/Game/DEV/UI/Cursor/M_ThrowTrajectory.M_ThrowTrajectory"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DecalMaterial(TEXT("/Game/DEV/UI/Cursor/M_ThrowLanding.M_ThrowLanding"));
	TrajectoryDotMesh = Dot.Object;
	TrajectoryMaterial = DotMaterial.Object;
	LandingMaterial = DecalMaterial.Object;
}

void UThrowAimComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<ACharacter>(GetOwner());
	Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>();
	Manipulation = GetOwner()->FindComponentByClass<UPhysicsManipulationComponent>();
	if (Character.IsValid()) AddTickPrerequisiteComponent(Character->GetCharacterMovement());
	if (Manipulation.IsValid()) Manipulation->AddTickPrerequisiteComponent(this);
}

void UThrowAimComponent::SetStickInput(FVector2D Value)
{
	StickInput = Value.GetClampedToMaxSize(1.0);
	if (StickInput.SizeSquared() <= FMath::Square(StickDeadZone))
	{
		StickDeviceSample = FVector2D::ZeroVector;
	}
	else if (!StickInput.Equals(StickDeviceSample, 0.05f))
	{
		bStickMoved = true;
		StickDeviceSample = StickInput;
	}
}

void UThrowAimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(DeltaTime, TickType, Tick);
	ACharacter* Pawn = Character.Get();
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const UPhysicalMontageComponent* Physics = Pawn ? Pawn->FindComponentByClass<UPhysicalMontageComponent>() : nullptr;
	if (!Pawn || !PC || !PC->IsLocalController() || !Interaction.IsValid() || !Manipulation.IsValid() || (Physics && Physics->IsPhysicsMontageActive()))
	{
		StopAiming();
		return;
	}
	const bool bDragging = Manipulation->IsDragging();
	if (bAiming && bDraggingAim != bDragging) StopAiming();
	const bool bPreparing = Manipulation->IsCarrying() && Interaction->IsPreparingRelease();
	const bool bInteractive = bDragging || bPreparing;
	const bool bFollowingThrough = bAiming && Interaction->IsPlayingReleaseAnimation();
	if (!bInteractive && !bFollowingThrough)
	{
		StopAiming();
		return;
	}

	UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
	FIntRect Bounds;
	FVector2D PlayerOrigin;
	if (!GetViewportBounds(*PC, Bounds, PlayerOrigin)) return;
	if (!bAiming)
	{
		bAiming = true;
		bDraggingAim = bDragging;
		bAimFrozen = false;
		LastCursorUpdateFrame = MAX_uint64;
		bGamepad = StickInput.SizeSquared() > FMath::Square(StickDeadZone);
		AimDirection = Pawn->GetActorForwardVector().GetSafeNormal2D();
		const FVector Ground = Pawn->GetActorLocation() - FVector::UpVector * Pawn->GetSimpleCollisionHalfHeight();
		if (!bHasScreenPosition && !PC->ProjectWorldLocationToScreen(Ground + AimDirection * AimDistance, ScreenPosition))
		{
			ScreenPosition = FVector2D(Bounds.Min + Bounds.Size() / 2);
		}
		FVector2D Mouse;
		if (PC->GetMousePosition(Mouse.X, Mouse.Y))
		{
			LastMousePosition = Mouse;
			if (!bGamepad && (PC->bShowMouseCursor || !PC->GetLocalPlayer()->ViewportClient->Viewport->HasMouseCapture())) ScreenPosition = Mouse;
		}
		bHasScreenPosition = true;
		if (!bDraggingAim)
		{
			bSavedOrientToMovement = Movement->bOrientRotationToMovement;
			bSavedControllerYaw = Pawn->bUseControllerRotationYaw;
			bSavedControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		}
		Reticle = bDraggingAim && ReticleWidgetClass ? CreateWidget<UUserWidget>(PC, ReticleWidgetClass) : nullptr;
		if (Reticle)
		{
			Reticle->SetIsFocusable(false);
			Reticle->SetVisibility(ESlateVisibility::HitTestInvisible);
			Reticle->AddToPlayerScreen(20);
			Reticle->SetDesiredSizeInViewport(ReticleSize);
			Reticle->SetAlignmentInViewport(FVector2D(0.5, 0.5));
		}
	}
	if (!bDraggingAim)
	{
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
		Pawn->bUseControllerRotationYaw = false;
	}

	if (bInteractive && !bAimFrozen) UpdateAim(*PC, DeltaTime);
	if (!bPreparing && bWasPreparing) bAimFrozen = true;
	bWasPreparing = bPreparing;
	bStickMoved = false;
	UpdateThrowPreview(bPreparing && !bAimFrozen, DeltaTime);
	if (Reticle)
	{
		Reticle->SetVisibility(bInteractive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Reticle->SetPositionInViewport(ScreenPosition - PlayerOrigin, true);
	}
	// Drag coordinates character facing with the simulated grip. The cursor only
	// supplies the desired direction here, so a blocked prop cannot twist the arms.
	if (bDraggingAim) return;
	if (bHasAimTarget)
	{
		const FVector ToTarget = (AimTarget - Pawn->GetActorLocation()).GetSafeNormal2D();
		if (!ToTarget.IsNearlyZero()) AimDirection = ToTarget;
	}
	const FRotator Target(0, AimDirection.Rotation().Yaw, 0);
	Pawn->SetActorRotation(FMath::RInterpTo(Pawn->GetActorRotation(), Target, DeltaTime, TurnSpeed));
}

bool UThrowAimComponent::GetViewportBounds(const APlayerController& Controller, FIntRect& Bounds, FVector2D& PlayerOrigin) const
{
	const ULocalPlayer* Player = Controller.GetLocalPlayer();
	if (!Player || !Player->ViewportClient || !Player->ViewportClient->Viewport) return false;
	FSceneViewProjectionData Projection;
	if (!Player->GetProjectionData(Player->ViewportClient->Viewport, Projection)) return false;
	Bounds = Projection.GetConstrainedViewRect();
	PlayerOrigin = FVector2D(Projection.GetViewRect().Min);
	return Bounds.Width() > 0 && Bounds.Height() > 0;
}

void UThrowAimComponent::UpdateAim(APlayerController& Controller, float DeltaTime, bool bRefreshGround)
{
	FIntRect Bounds;
	FVector2D PlayerOrigin;
	if (!GetViewportBounds(Controller, Bounds, PlayerOrigin)) return;
	FVector2D Mouse;
	const bool bHasMouse = Controller.GetMousePosition(Mouse.X, Mouse.Y);
	const bool bAbsoluteMouse = Controller.bShowMouseCursor || !Controller.GetLocalPlayer()->ViewportClient->Viewport->HasMouseCapture();
	if (LastCursorUpdateFrame != GFrameCounter)
	{
		FVector2D MouseDelta = FVector2D::ZeroVector;
		if (Controller.PlayerInput)
		{
			MouseDelta = FVector2D(Controller.PlayerInput->GetRawKeyValue(EKeys::MouseX), -Controller.PlayerInput->GetRawKeyValue(EKeys::MouseY));
		}
		const bool bMouseMoved = bAbsoluteMouse ? bHasMouse && FVector2D::DistSquared(Mouse, LastMousePosition) > 1.0 : MouseDelta.SizeSquared() > 0.04;
		if (bMouseMoved)
			bGamepad = false;
		else if (bStickMoved)
			bGamepad = true;
		if (bGamepad)
		{
			const double Magnitude = StickInput.Size();
			if (Magnitude > StickDeadZone)
			{
				const double Strength = FMath::Pow((Magnitude - StickDeadZone) / (1.0 - StickDeadZone), StickCursorExponent);
				const FVector2D Direction(StickInput.X / Magnitude, -StickInput.Y / Magnitude);
				ScreenPosition += Direction * (Strength * StickCursorSpeed * Bounds.Height() / 1080.0 * DeltaTime);
			}
		}
		else if (bAbsoluteMouse && bHasMouse)
			ScreenPosition = Mouse;
		else
			ScreenPosition += MouseDelta * (MouseSensitivity * Bounds.Height() / 1080.0);
		LastCursorUpdateFrame = GFrameCounter;
		bStickMoved = false;
	}
	else if (!bGamepad && bAbsoluteMouse && bHasMouse)
	{
		ScreenPosition = Mouse;
	}
	const double Margin = FMath::Min3(24.0, Bounds.Width() * 0.25, Bounds.Height() * 0.25);
	ScreenPosition.X = FMath::Clamp(ScreenPosition.X, Bounds.Min.X + Margin, Bounds.Max.X - Margin);
	ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y, Bounds.Min.Y + Margin, Bounds.Max.Y - Margin);
	if (bGamepad && bAbsoluteMouse)
	{
		const FIntPoint Pixel(FMath::RoundToInt(ScreenPosition.X), FMath::RoundToInt(ScreenPosition.Y));
		Controller.SetMouseLocation(Pixel.X, Pixel.Y);
		LastMousePosition = FVector2D(Pixel);
	}
	else if (bHasMouse)
		LastMousePosition = Mouse;

	FVector RayStart, RayDirection;
	bHasAimTarget = Controller.DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayStart, RayDirection);
	if (!bHasAimTarget)
	{
		Manipulation->ClearThrowTarget();
		return;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ThrowCursor), true, GetOwner());
	Query.AddIgnoredComponent(Manipulation->GetManipulatedComponent());
	FHitResult Hit;
	const FVector RayEnd = RayStart + RayDirection * AimTraceDistance;
	const double FloorZ = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
	const double FloorDistance = FMath::Abs(RayDirection.Z) > 0.001 ? (FloorZ - RayStart.Z) / RayDirection.Z : -1.0;
	const bool bReachesFloor = FloorDistance > 0.0 && FloorDistance <= AimTraceDistance;
	AimTarget = bReachesFloor ? RayStart + RayDirection * FloorDistance : RayEnd;
	if (bDraggingAim)
	{
		if (GetWorld()->LineTraceSingleByChannel(Hit, RayStart, RayEnd, AimTraceChannel, Query)) AimTarget = Hit.ImpactPoint;
	}
	else if (bReachesFloor)
	{
		if (bRefreshGround || GetWorld()->GetTimeSeconds() >= NextThrowPreviewAt)
		{
			// Keep the projected cursor responsive between floor probes, even beyond scenery.
			AimGroundHeightOffset = 0.0;
			AimSurfaceNormal = FVector::UpVector;
			constexpr double FloorProbeHeight = 35.0;
			const FVector Vertical = FVector::UpVector * FloorProbeHeight;
			if (GetWorld()->LineTraceSingleByChannel(Hit, AimTarget + Vertical, AimTarget - Vertical, AimTraceChannel, Query) && Hit.ImpactNormal.Z >= 0.7)
			{
				AimGroundHeightOffset = Hit.ImpactPoint.Z - AimTarget.Z;
				AimSurfaceNormal = Hit.ImpactNormal;
			}
		}
		AimTarget.Z += AimGroundHeightOffset;
	}
	else
	{
		bHasAimTarget = false;
		Manipulation->ClearThrowTarget();
		return;
	}
#if ENABLE_DRAW_DEBUG
	if (!bDraggingAim && CVarDebugThrowTrace.GetValueOnGameThread() != 0)
	{
		DrawDebugLine(GetWorld(), RayStart, AimTarget, FColor::Cyan);
		DrawDebugSphere(GetWorld(), AimTarget, 5.f, 8, FColor::Cyan);
	}
#endif
	if (!bDraggingAim) Manipulation->SetThrowTargetFromView(AimTarget, AimSurfaceNormal, RayDirection);
}

void UThrowAimComponent::UpdateThrowPreview(bool bShow, float DeltaTime)
{
	if (!bShow)
	{
		NextThrowPreviewAt = 0.0;
		PreviewBlendRemaining = 0.0;
		PreviewPath.Reset();
		DisplayedDots.Reset();
		PreviewImpact = {};
		if (TrajectoryDots) TrajectoryDots->SetVisibility(false);
		if (TargetDecal) TargetDecal->SetVisibility(false);
		if (ImpactDecal) ImpactDecal->SetVisibility(false);
		return;
	}
	if (bHasAimTarget && !TargetDecal && LandingMaterial) TargetDecal = CreateThrowDecal(TEXT("ThrowTarget"));
	if (TargetDecal)
	{
		TargetDecal->SetVisibility(bHasAimTarget);
		if (bHasAimTarget) TargetDecal->SetWorldLocationAndRotation(AimTarget, (-AimSurfaceNormal).Rotation());
	}

	constexpr double PreviewInterval = 1.0 / 20.0;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextThrowPreviewAt)
	{
		if (PreviewBlendRemaining > 0.0) DrawThrowTrajectory(DeltaTime);
		return;
	}
	NextThrowPreviewAt += PreviewInterval;
	if (NextThrowPreviewAt <= Now) NextThrowPreviewAt = Now + PreviewInterval;

	TArray<FVector> Path;
	FHitResult Hit;
	bool bBlocked = false;
	bool bUsesHighArc = false;
	bool bDrawDebug = false;
#if ENABLE_DRAW_DEBUG
	bDrawDebug = CVarDebugThrowTrace.GetValueOnGameThread() != 0;
#endif
	if (!Manipulation->PredictThrow(Interaction->GetInteractionHoldDuration(), Path, Hit, bDrawDebug, &bBlocked, &bUsesHighArc))
	{
		PreviewPath.Reset();
		PreviewBlendRemaining = 0.0;
		if (TrajectoryDots) TrajectoryDots->SetVisibility(false);
		if (ImpactDecal) ImpactDecal->SetVisibility(false);
		return;
	}
	// Switching arcs or impact surfaces can put intermediate points inside an obstacle.
	const bool bSameImpactSurface = !bBlocked || (Hit.GetComponent() == PreviewImpact.GetComponent()
	    && FVector::DotProduct(Hit.ImpactNormal, PreviewImpact.ImpactNormal) >= 0.95);
	const bool bCanBlend = !PreviewPath.IsEmpty() && bBlocked == bPreviewBlocked && bSameImpactSurface && bUsesHighArc == bPreviewUsesHighArc;
	PreviewBlendRemaining = bCanBlend ? PreviewInterval : 0.0;
	PreviewPath = MoveTemp(Path);
	PreviewImpact = Hit;
	bPreviewBlocked = bBlocked;
	bPreviewUsesHighArc = bUsesHighArc;

	if (!TrajectoryDots && TrajectoryDotMesh && TrajectoryMaterial)
	{
		TrajectoryDots = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("ThrowTrajectory"));
		TrajectoryDots->SetStaticMesh(TrajectoryDotMesh);
		TrajectoryDots->SetMaterial(0, TrajectoryMaterial);
		TrajectoryDots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		TrajectoryDots->SetCanEverAffectNavigation(false);
		TrajectoryDots->SetCastShadow(false);
		TrajectoryDots->RegisterComponent();
	}

	if (bBlocked && !ImpactDecal && LandingMaterial)
	{
		ImpactDecal = CreateThrowDecal(TEXT("ThrowImpact"));
		if (auto* Material = ImpactDecal->CreateDynamicMaterialInstance())
			Material->SetVectorParameterValue(TEXT("MarkerColor"), FLinearColor(2.0f, 0.05f, 0.025f));
	}
	if (ImpactDecal) ImpactDecal->SetVisibility(bBlocked);
	DrawThrowTrajectory(DeltaTime);
}

void UThrowAimComponent::DrawThrowTrajectory(float DeltaTime)
{
	const double Alpha = PreviewBlendRemaining > DeltaTime ? DeltaTime / PreviewBlendRemaining : 1.0;
	PreviewBlendRemaining = FMath::Max(PreviewBlendRemaining - DeltaTime, 0.0);
	if (TrajectoryDots)
	{
		const int32 PreviousCount = DisplayedDots.Num();
		const int32 Count = PreviewPath.Num() - 2;
		const FVector Tail = PreviousCount > 0 ? DisplayedDots.Last().GetLocation() : PreviewPath[0];
		DisplayedDots.SetNum(Count);
		bool bChanged = PreviousCount != Count;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FTransform& Dot = DisplayedDots[Index];
			const FVector& Target = PreviewPath[Index + 1];
			if (Index >= PreviousCount) Dot = FTransform(FQuat::Identity, Tail, FVector(0.10));
			const FVector Location = FMath::Lerp(Dot.GetLocation(), Target, Alpha);
			if (Dot.GetLocation().Equals(Location, 0.05)) continue;
			Dot.SetLocation(Location);
			bChanged = true;
		}
		if (TrajectoryDots->GetInstanceCount() != Count)
		{
			TrajectoryDots->ClearInstances();
			TrajectoryDots->AddInstances(DisplayedDots, false, true, false);
		}
		else if (bChanged && Count > 0)
		{
			TrajectoryDots->BatchUpdateInstancesTransforms(0, DisplayedDots, true);
		}
		TrajectoryDots->SetVisibility(Count > 0);
	}
	if (ImpactDecal && bPreviewBlocked)
	{
		const FVector Location = FMath::Lerp(ImpactDecal->GetComponentLocation(), FVector(PreviewImpact.ImpactPoint), Alpha);
		const FQuat Rotation = FQuat::Slerp(ImpactDecal->GetComponentQuat(), (-PreviewImpact.ImpactNormal).ToOrientationQuat(), Alpha);
		ImpactDecal->SetWorldLocationAndRotation(Location, Rotation);
	}
}

UDecalComponent* UThrowAimComponent::CreateThrowDecal(FName Name) const
{
	auto* Decal = NewObject<UDecalComponent>(GetOwner(), Name);
	Decal->SetDecalMaterial(LandingMaterial);
	Decal->DecalSize = FVector(18.0, 46.0, 46.0);
	Decal->SortOrder = 20;
	Decal->SetFadeScreenSize(0.001f);
	Decal->RegisterComponent();
	return Decal;
}

void UThrowAimComponent::FreezeAim()
{
	if (!bAiming || bAimFrozen || bDraggingAim) return;
	APlayerController* Controller = Character.IsValid() ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (Controller && Controller->IsLocalController()) UpdateAim(*Controller, GetWorld()->GetDeltaSeconds(), true);
	bAimFrozen = true;
}

bool UThrowAimComponent::GetAimTarget(FVector& WorldLocation) const
{
	WorldLocation = AimTarget;
	return bHasAimTarget;
}

void UThrowAimComponent::StopAiming()
{
	if (!bAiming) return;
	if (ACharacter* Pawn = Character.Get(); Pawn && !bDraggingAim)
	{
		Pawn->GetCharacterMovement()->bOrientRotationToMovement = bSavedOrientToMovement;
		Pawn->GetCharacterMovement()->bUseControllerDesiredRotation = bSavedControllerDesiredRotation;
		Pawn->bUseControllerRotationYaw = bSavedControllerYaw;
	}
	if (Reticle) Reticle->RemoveFromParent();
	Reticle = nullptr;
	UpdateThrowPreview(false);
	bAiming = bDraggingAim = bWasPreparing = bStickMoved = false;
	bAimFrozen = bHasAimTarget = false;
	if (Manipulation.IsValid()) Manipulation->ClearThrowTarget();
}

void UThrowAimComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopAiming();
	if (TrajectoryDots) TrajectoryDots->DestroyComponent();
	if (TargetDecal) TargetDecal->DestroyComponent();
	if (ImpactDecal) ImpactDecal->DestroyComponent();
	Super::EndPlay(Reason);
}
