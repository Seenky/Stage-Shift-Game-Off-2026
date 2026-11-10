#include "Components/Interaction/ThrowAimComponent.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "SceneView.h"
#include "UI/ThrowAimWidget.h"

UThrowAimComponent::UThrowAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	ReticleWidgetClass = UThrowAimWidget::StaticClass();
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
	if (!Pawn || !PC || !PC->IsLocalController() || !Interaction.IsValid() || !Manipulation.IsValid() ||
		(Physics && Physics->IsPhysicsMontageActive()))
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
		Reticle = ReticleWidgetClass ? CreateWidget<UUserWidget>(PC, ReticleWidgetClass) : nullptr;
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

void UThrowAimComponent::UpdateAim(APlayerController& Controller, float DeltaTime)
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
		if (bMouseMoved) bGamepad = false;
		else if (bStickMoved) bGamepad = true;
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
		else if (bAbsoluteMouse && bHasMouse) ScreenPosition = Mouse;
		else ScreenPosition += MouseDelta * MouseSensitivity;
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
	else if (bHasMouse) LastMousePosition = Mouse;

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
	FVector SurfaceNormal = FVector::UpVector;
	if (GetWorld()->LineTraceSingleByChannel(Hit, RayStart, RayStart + RayDirection * AimTraceDistance, AimTraceChannel, Query))
	{
		AimTarget = Hit.ImpactPoint;
		SurfaceNormal = Hit.ImpactNormal;
	}
	else
	{
		const double FloorZ = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
		const double Distance = FMath::Abs(RayDirection.Z) > 0.001 ? (FloorZ - RayStart.Z) / RayDirection.Z : -1.0;
		AimTarget = RayStart + RayDirection * (Distance > 0.0 ? FMath::Min(Distance, static_cast<double>(AimTraceDistance)) : AimTraceDistance);
	}
	if (!Manipulation->IsDragging()) Manipulation->SetThrowTarget(AimTarget, SurfaceNormal);
}

void UThrowAimComponent::FreezeAim()
{
	if (!bAiming || bAimFrozen || bDraggingAim) return;
	APlayerController* Controller = Character.IsValid() ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (Controller && Controller->IsLocalController()) UpdateAim(*Controller, GetWorld()->GetDeltaSeconds());
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
	bAiming = bDraggingAim = bWasPreparing = bStickMoved = false;
	bAimFrozen = bHasAimTarget = false;
	if (Manipulation.IsValid()) Manipulation->ClearThrowTarget();
}

void UThrowAimComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopAiming();
	Super::EndPlay(Reason);
}
