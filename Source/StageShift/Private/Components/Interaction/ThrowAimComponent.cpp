#include "Components/Interaction/ThrowAimComponent.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Physics/PhysicsManipulationComponent.h"
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
}

void UThrowAimComponent::SetStickInput(FVector2D Value)
{
	if (!Value.Equals(StickInput, 0.05f) && Value.SizeSquared() > FMath::Square(StickDeadZone)) bStickMoved = true;
	StickInput = Value;
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
	const bool bPreparing = Manipulation->IsCarrying() && Interaction->IsPreparingRelease();
	const bool bFollowingThrough = bAiming && Interaction->IsPlayingReleaseAnimation();
	if (!bPreparing && !bFollowingThrough)
	{
		StopAiming();
		return;
	}

	UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
	if (!bAiming)
	{
		bAiming = true;
		bGamepad = StickInput.SizeSquared() > FMath::Square(StickDeadZone);
		AimDirection = Pawn->GetActorForwardVector().GetSafeNormal2D();
		const FVector Ground = Pawn->GetActorLocation() - FVector::UpVector * Pawn->GetSimpleCollisionHalfHeight();
		PC->ProjectWorldLocationToScreen(Ground + AimDirection * AimDistance, ScreenPosition);
		bSavedOrientToMovement = Movement->bOrientRotationToMovement;
		bSavedControllerYaw = Pawn->bUseControllerRotationYaw;
		bSavedControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
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
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = false;
	Pawn->bUseControllerRotationYaw = false;

	if (bPreparing)
	{
		float MouseX = 0.0f, MouseY = 0.0f;
		PC->GetInputMouseDelta(MouseX, MouseY);
		const bool bMouseMoved = FVector2D(MouseX, MouseY).SizeSquared() > 0.04f;
		if (bMouseMoved) bGamepad = false;
		else if (bStickMoved) bGamepad = true;

		if (bGamepad)
		{
			if (StickInput.SizeSquared() > FMath::Square(StickDeadZone))
			{
				const float CameraYaw = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Yaw : PC->GetControlRotation().Yaw;
				const FRotationMatrix Basis(FRotator(0, CameraYaw, 0));
				AimDirection = (Basis.GetUnitAxis(EAxis::X) * StickInput.Y + Basis.GetUnitAxis(EAxis::Y) * StickInput.X).GetSafeNormal2D();
			}
			const FVector Ground = Pawn->GetActorLocation() - FVector::UpVector * Pawn->GetSimpleCollisionHalfHeight();
			PC->ProjectWorldLocationToScreen(Ground + AimDirection * AimDistance, ScreenPosition);
		}
		else
		{
			ScreenPosition += FVector2D(MouseX, -MouseY) * MouseSensitivity;
			int32 Width = 0, Height = 0;
			PC->GetViewportSize(Width, Height);
			if (Width > 48 && Height > 48)
			{
				ScreenPosition.X = FMath::Clamp(ScreenPosition.X, 24.0, static_cast<double>(Width - 24));
				ScreenPosition.Y = FMath::Clamp(ScreenPosition.Y, 24.0, static_cast<double>(Height - 24));
			}
			FVector RayStart, RayDirection;
			// A plane gives stable horizontal aim even over props and uneven geometry.
			if (PC->DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, RayStart, RayDirection) && FMath::Abs(RayDirection.Z) > 0.001f)
			{
				const float FloorZ = Pawn->GetActorLocation().Z - Pawn->GetSimpleCollisionHalfHeight();
				const double Distance = (FloorZ - RayStart.Z) / RayDirection.Z;
				const FVector Offset = RayStart + RayDirection * Distance - Pawn->GetActorLocation();
				if (Distance > 0 && Offset.SizeSquared2D() > FMath::Square(25.0)) AimDirection = Offset.GetSafeNormal2D();
			}
		}
		Manipulation->SetThrowDirection(AimDirection);
	}
	// Input release freezes the target direction through the release timer and follow-through.
	if (!bPreparing && bWasPreparing) Manipulation->SetThrowDirection(AimDirection);
	bWasPreparing = bPreparing;
	bStickMoved = false;
	if (Reticle)
	{
		Reticle->SetVisibility(bPreparing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Reticle->SetPositionInViewport(ScreenPosition, true);
	}
	const FRotator Target(0, AimDirection.Rotation().Yaw, 0);
	Pawn->SetActorRotation(FMath::RInterpTo(Pawn->GetActorRotation(), Target, DeltaTime, TurnSpeed));
}

void UThrowAimComponent::StopAiming()
{
	if (!bAiming) return;
	if (ACharacter* Pawn = Character.Get())
	{
		Pawn->GetCharacterMovement()->bOrientRotationToMovement = bSavedOrientToMovement;
		Pawn->GetCharacterMovement()->bUseControllerDesiredRotation = bSavedControllerDesiredRotation;
		Pawn->bUseControllerRotationYaw = bSavedControllerYaw;
	}
	if (Reticle) Reticle->RemoveFromParent();
	Reticle = nullptr;
	bAiming = bWasPreparing = bStickMoved = false;
}

void UThrowAimComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StopAiming();
	Super::EndPlay(Reason);
}
