#include "Components/Interaction/ThrowAimComponent.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Physics/PhysicsActor.h"
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
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Tape(TEXT("/Game/DEV/UI/Cursor/ThrowTape/NS_ThrowTape.NS_ThrowTape"));
	ThrowTapeSystem = Tape.Object;
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
	if (!GetViewportBounds(*PC, Bounds)) return;
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
		const auto* DraggedComponent = bDraggingAim ? Manipulation->GetManipulatedComponent() : nullptr;
		const auto* DraggedProp = DraggedComponent ? Cast<AStagePhysicsActor>(DraggedComponent->GetOwner()) : nullptr;
		const bool bShowDragReticle = bDraggingAim && !(DraggedProp && DraggedProp->IsCurtainLever());
		if (bShowDragReticle && ReticleWidgetClass) CreateReticle(*PC, ReticleWidgetClass);
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
	// Drag coordinates character facing with the simulated grip. The cursor only
	// supplies the desired direction here, so a blocked prop cannot twist the arms.
	if (!bDraggingAim)
	{
		if (bHasAimTarget)
		{
			const FVector ToTarget = (AimTarget - Pawn->GetActorLocation()).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero()) AimDirection = ToTarget;
		}
		const FRotator Target(0, AimDirection.Rotation().Yaw, 0);
		Pawn->SetActorRotation(FMath::RInterpTo(Pawn->GetActorRotation(), Target, DeltaTime, TurnSpeed));
		UpdateThrowPreview(Manipulation->IsCarrying() && (bPreparing || bFollowingThrough), DeltaTime);
		if (bPreviewBlocked && bHasAimTarget && !Reticle)
		{
			CreateReticle(*PC, UThrowAimWidget::StaticClass());
			if (auto* Widget = Cast<UThrowAimWidget>(Reticle)) Widget->SetBlocked();
		}
	}
	if (Reticle)
	{
		FVector2D ReticlePosition = ScreenPosition - FVector2D(Bounds.Min);
		const bool bVisible = bDraggingAim ? bInteractive : bPreviewBlocked && bHasAimTarget
		    && PC->ProjectWorldLocationToScreen(AimTarget, ReticlePosition, true);
		Reticle->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bVisible) Reticle->SetPositionInViewport(ReticlePosition, true);
	}
}

void UThrowAimComponent::CreateReticle(APlayerController& Controller, TSubclassOf<UUserWidget> WidgetClass)
{
	Reticle = CreateWidget<UUserWidget>(&Controller, WidgetClass);
	if (!Reticle) return;
	Reticle->SetIsFocusable(false);
	Reticle->AddToPlayerScreen(20);
	Reticle->SetDesiredSizeInViewport(ReticleSize);
	Reticle->SetAlignmentInViewport(FVector2D(0.5, 0.5));
}

bool UThrowAimComponent::GetViewportBounds(const APlayerController& Controller, FIntRect& Bounds) const
{
	const ULocalPlayer* Player = Controller.GetLocalPlayer();
	if (!Player || !Player->ViewportClient || !Player->ViewportClient->Viewport) return false;
	FSceneViewProjectionData Projection;
	if (!Player->GetProjectionData(Player->ViewportClient->Viewport, Projection)) return false;
	Bounds = Projection.GetConstrainedViewRect();
	return Bounds.Width() > 0 && Bounds.Height() > 0;
}

void UThrowAimComponent::UpdateAim(APlayerController& Controller, float DeltaTime, bool bRefreshGround)
{
	FIntRect Bounds;
	if (!GetViewportBounds(Controller, Bounds)) return;
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
		DisplayedPath.Reset();
		PreviewImpact = {};
		bPreviewBlocked = false;
		HideThrowTape();
		return;
	}
	constexpr double PreviewInterval = 1.0 / 20.0;
	const double Now = GetWorld()->GetTimeSeconds();
	// Sample the last visible release frames before the timer detaches the prop.
	if (Now < NextThrowPreviewAt && !(bAimFrozen && Interaction->IsReleaseImminent()))
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
		bPreviewBlocked = false;
		HideThrowTape();
		return;
	}
	if (Hit.bBlockingHit && !bBlocked)
	{
		// The prop hits simple collision, which can sit below the rendered floorboards.
		constexpr double SurfaceProbeHalfHeight = 12.0;
		const FVector Probe = FVector::UpVector * SurfaceProbeHalfHeight;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(ThrowLandingSurface), true, GetOwner());
		Query.AddIgnoredComponent(Manipulation->GetManipulatedComponent());
		FHitResult SurfaceHit;
		if (GetWorld()->LineTraceSingleByChannel(SurfaceHit, Hit.ImpactPoint + Probe, Hit.ImpactPoint - Probe, AimTraceChannel, Query)
		    && SurfaceHit.ImpactNormal.Z >= 0.7)
		{
			Hit.ImpactPoint = SurfaceHit.ImpactPoint;
			Hit.ImpactNormal = SurfaceHit.ImpactNormal;
		}
		// The sweep follows the prop's center. Ease the preview toward the surface contact it marks.
		const FVector Contact = Hit.ImpactPoint + Hit.ImpactNormal;
		const FVector ContactOffset = Contact - Path.Last();
		const int32 LastIndex = Path.Num() - 1;
		const int32 ApproachSamples = FMath::Min(LastIndex, 4);
		for (int32 Sample = 1; Sample <= ApproachSamples; ++Sample)
		{
			const double Fraction = static_cast<double>(Sample) / ApproachSamples;
			Path[LastIndex - ApproachSamples + Sample] += ContactOffset * FMath::Square(Fraction);
		}
		if (FVector::DistSquared(Path[LastIndex - 1], Contact) > FMath::Square(45.0))
			Path.Insert(FMath::Lerp(Path[LastIndex - 1], Contact, 0.5), LastIndex);
	}
	// Switching arcs or impact surfaces can put intermediate points inside an obstacle.
	const bool bSameImpactSurface = Hit.bBlockingHit == PreviewImpact.bBlockingHit &&
	    (!Hit.bBlockingHit || (Hit.GetComponent() == PreviewImpact.GetComponent()
	    && FVector::DotProduct(Hit.ImpactNormal, PreviewImpact.ImpactNormal) >= 0.95));
	const bool bCanBlend = !bAimFrozen && !PreviewPath.IsEmpty() && bBlocked == bPreviewBlocked
	    && bSameImpactSurface && bUsesHighArc == bPreviewUsesHighArc;
	PreviewBlendRemaining = bCanBlend ? PreviewInterval : 0.0;
	PreviewPath = MoveTemp(Path);
	PreviewImpact = Hit;
	bPreviewBlocked = bBlocked;
	bPreviewUsesHighArc = bUsesHighArc;

	DrawThrowTrajectory(DeltaTime);
}

void UThrowAimComponent::DrawThrowTrajectory(float DeltaTime)
{
	const double Alpha = PreviewBlendRemaining > DeltaTime ? DeltaTime / PreviewBlendRemaining : 1.0;
	PreviewBlendRemaining = FMath::Max(PreviewBlendRemaining - DeltaTime, 0.0);
	if (bPreviewBlocked || !ThrowTapeSystem)
	{
		HideThrowTape();
		return;
	}

	const APlayerController* Controller = Character.IsValid() ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!Controller) return;
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const int32 PreviousCount = DisplayedPath.Num();
	const FVector Tail = PreviousCount > 0 ? DisplayedPath.Last() : PreviewPath[0];
	DisplayedPath.SetNumUninitialized(PreviewPath.Num());
	double PathLength = 0.0;
	for (int32 Index = 0; Index < PreviewPath.Num(); ++Index)
	{
		const FVector Previous = Index < PreviousCount ? DisplayedPath[Index] : Tail;
		DisplayedPath[Index] = FMath::Lerp(Previous, PreviewPath[Index], Alpha);
		if (Index > 0) PathLength += FVector::Distance(DisplayedPath[Index - 1], DisplayedPath[Index]);
	}

	// NS_ThrowTape spawns 40 strips, including nine for the landing mark and one hidden fallback.
	constexpr int32 MaxArcStrips = 30;
	constexpr double TargetStripSpacing = 80.0;
	const int32 ArcStrips = FMath::Clamp(FMath::RoundToInt(PathLength / TargetStripSpacing), 1, MaxArcStrips);
	const double Spacing = PathLength / (ArcStrips + 1);
	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<FVector> Scales;
	Positions.Reserve(ArcStrips + 10);
	Rotations.Reserve(ArcStrips + 10);
	Scales.Reserve(ArcStrips + 10);
	const auto AddStrip = [&](const FVector& Position, const FQuat& Rotation, double Length, double Width)
	{
		Positions.Add(Position);
		Rotations.Add(Rotation);
		Scales.Emplace(Length / 100.0, Width / 100.0, 1.0); // The Niagara mesh is a 100 cm plane.
	};

	double Distance = 0.0;
	int32 StripIndex = 1;
	for (int32 Index = 1; Index < DisplayedPath.Num() && StripIndex <= ArcStrips; ++Index)
	{
		const FVector& Start = DisplayedPath[Index - 1];
		const FVector Segment = DisplayedPath[Index] - Start;
		const double SegmentLength = Segment.Size();
		if (SegmentLength <= UE_KINDA_SMALL_NUMBER) continue;
		while (StripIndex <= ArcStrips && Spacing * StripIndex <= Distance + SegmentLength)
		{
			const FVector Position = Start + Segment * ((Spacing * StripIndex - Distance) / SegmentLength);
			const FVector Tangent = Segment / SegmentLength;
			const FVector ToView = ViewLocation - Position;
			const double Length = FMath::Clamp(Spacing * 0.56, 15.0, 47.0);
			AddStrip(Position, FRotationMatrix::MakeFromXZ(Tangent, ToView).ToQuat(), Length, Length * 0.33);
			++StripIndex;
		}
		Distance += SegmentLength;
	}

	if (PreviewImpact.bBlockingHit)
	{
		const FVector Center = DisplayedPath.Last();
		const FVector Normal = PreviewImpact.ImpactNormal;
		const FMatrix Surface = FRotationMatrix::MakeFromZX(Normal, FVector::ForwardVector);
		const FVector X = Surface.GetScaledAxis(EAxis::X);
		const FVector Y = Surface.GetScaledAxis(EAxis::Y);
		const FQuat AlongX = Surface.ToQuat();
		const FQuat AlongY = FQuat(Normal, UE_HALF_PI) * AlongX;
		constexpr double HalfExtent = 47.0;
		constexpr double Length = 28.0;
		constexpr double Width = 12.5;
		for (const double SX : {-1.0, 1.0})
		{
			for (const double SY : {-1.0, 1.0})
			{
				const FVector Corner = Center + HalfExtent * (SX * X + SY * Y);
				AddStrip(Corner - (Length - Width) * 0.5 * SX * X, AlongX, Length, Width);
				AddStrip(Corner - (Length - Width) * 0.5 * SY * Y + 0.2 * Normal, AlongY, Length, Width);
			}
		}
		AddStrip(Center, FQuat(Normal, UE_PI * 0.25) * AlongX, 25.5, 25.5);
	}

	// Array lookups beyond the used slots clamp to the final, invisible strip.
	Positions.Add(DisplayedPath.Last());
	Rotations.Add(FQuat::Identity);
	Scales.Add(FVector::ZeroVector);
	if (!ThrowTape)
	{
		ThrowTape = NewObject<UNiagaraComponent>(GetOwner(), TEXT("ThrowTapePreview"));
		ThrowTape->SetAutoActivate(false);
		ThrowTape->SetAsset(ThrowTapeSystem);
		ThrowTape->SetCastShadow(false);
		ThrowTape->SetWorldLocation(DisplayedPath[0]);
		ThrowTape->SetTickBehavior(ENiagaraTickBehavior::UseComponentTickGroup);
		ThrowTape->SetTickGroup(TG_PostPhysics);
		ThrowTape->RegisterComponent();
	}
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(ThrowTape, TEXT("User.TapePositions"), Positions);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(ThrowTape, TEXT("User.TapeRotations"), Rotations);
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(ThrowTape, TEXT("User.TapeScales"), Scales);
	ThrowTape->SetVisibility(true);
	if (!ThrowTape->IsActive()) ThrowTape->Activate(true);
}

void UThrowAimComponent::HideThrowTape()
{
	if (!ThrowTape) return;
	ThrowTape->SetVisibility(false);
	if (ThrowTape->IsActive()) ThrowTape->DeactivateImmediate();
}

void UThrowAimComponent::FreezeAim()
{
	if (!bAiming || bAimFrozen || bDraggingAim) return;
	APlayerController* Controller = Character.IsValid() ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (Controller && Controller->IsLocalController()) UpdateAim(*Controller, GetWorld()->GetDeltaSeconds(), true);
	bAimFrozen = true;
	// The release animation can still move the prop before it leaves the hand.
	NextThrowPreviewAt = 0.0;
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
	if (ThrowTape) ThrowTape->DestroyComponent();
	Super::EndPlay(Reason);
}
