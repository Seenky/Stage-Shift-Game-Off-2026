#include "Stage/Environment/StageSpotlightControl.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Stage/Environment/StageControlLeverComponent.h"
#include "Stage/Environment/StageControlMeshComponent.h"

AStageSpotlightControl::AStageSpotlightControl()
{
	PrimaryActorTick.bCanEverTick = true;
	auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Interaction"));
	SetRootComponent(Root);

	ControlMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ControlMesh"));
	ControlMesh->SetupAttachment(Root);
	ControlMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	ControlLever = CreateDefaultSubobject<UStageControlLeverComponent>(TEXT("ControlLever"));
	ControlLever->SetupAttachment(ControlMesh);
	ControlVisual = CreateDefaultSubobject<UStageControlMeshComponent>(TEXT("ControlVisual"));
	ControlVisual->SetupAttachment(Root);
	Spotlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Spotlight"));
	Spotlight->SetupAttachment(Root);
	Spotlight->SetMobility(EComponentMobility::Movable);
	Spotlight->SetOuterConeAngle(25.f);
	Spotlight->SetInnerConeAngle(14.f);
	Spotlight->SetAttenuationRadius(1600.f);
}

const UMeshComponent* AStageSpotlightControl::GetInteractionMesh() const
{
	return ControlVisual->GetSkinnedAsset() ? static_cast<const UMeshComponent*>(ControlVisual.Get()) : ControlMesh.Get();
}

void AStageSpotlightControl::BeginPlay()
{
	Super::BeginPlay();
	ControlLever->BindVisual(*ControlVisual, TEXT("light_lever"), false);
	BaseRotation = Spotlight->GetRelativeRotation();
	BaseIntensity = Spotlight->Intensity;
	Spotlight->SetIntensity(0.f);
	SetCueTarget(InitialCueId, InitialTarget.Get());
}

bool AStageSpotlightControl::CanInteract_Implementation(const FInteractionContext& Context) const
{
	if (const auto* Component = Context.Hit.GetComponent(); Component && Component != ControlMesh.Get()) return false;
	const auto* Pawn = Cast<APawn>(Context.Interactor);
	return Target.IsValid() && Pawn && Pawn->IsPlayerControlled()
	    && FVector::DistSquared2D(Pawn->GetActorLocation(), ControlMesh->GetComponentLocation()) <= FMath::Square(OperatorRange)
	    && (!AimOperator.IsValid() || AimOperator.Get() == Pawn);
}

EInteractionResult AStageSpotlightControl::OnInteraction_Implementation(const FInteractionContext& Context)
{
	if (Context.InteractionPhase != EInteractionPhase::Start || !CanInteract_Implementation(Context)) return EInteractionResult::Rejected;
	if (State == EStageSpotlightState::Aiming) CommitAim();
	else
	{
		AimOperator = Context.Interactor;
		LastCursorPosition.Reset();
		SetState(EStageSpotlightState::Aiming);
		if (auto* Interaction = Context.Interactor->FindComponentByClass<UInteractionComponent>())
			ControlLever->PlayHandMotion(*Interaction, EInteractionHandMotion::Hold);
	}
	if (ActivationSound) UGameplayStatics::PlaySoundAtLocation(this, ActivationSound, ControlMesh->GetComponentLocation());
	return EInteractionResult::Completed;
}

FName AStageSpotlightControl::GetInteractionAnimationId_Implementation() const
{
	static const FName Id(TEXT("StageSpotlightLever"));
	return Id;
}

void AStageSpotlightControl::SetCueTarget(FName InCueId, AActor* InTarget)
{
	if (CueOwner.IsValid()) return;
	KeepLightUntil = 0.0;
	ApplyCueTarget(InCueId, InTarget);
}

bool AStageSpotlightControl::ClaimCue(UObject& Requester, FName InCueId, AActor* InTarget)
{
	if (CueOwner.IsValid() || IsActorBeingDestroyed() || !IsValid(InTarget) || InTarget->IsActorBeingDestroyed() ||
	    InTarget->GetWorld() != GetWorld()) return false;
	CueOwner = &Requester;
	KeepLightUntil = 0.0;
	ApplyCueTarget(InCueId, InTarget);
	return true;
}

void AStageSpotlightControl::ReleaseCue(const UObject& Requester, bool bSucceeded)
{
	if (CueOwner.Get() != &Requester) return;
	CueOwner.Reset();
	constexpr double ConfirmationHoldDuration = 0.6;
	KeepLightUntil = bSucceeded ? GetWorld()->GetTimeSeconds() + ConfirmationHoldDuration : 0.0;
	ApplyCueTarget(NAME_None, nullptr);
}

void AStageSpotlightControl::ApplyCueTarget(FName InCueId, AActor* InTarget)
{
	ReleaseAimOperator();
	bAimCommittedForCue = false;
	Target = InTarget;
	CueId = InCueId;
	SetState(Target.IsValid() ? EStageSpotlightState::Ready : EStageSpotlightState::Idle);
}

void AStageSpotlightControl::CommitAim()
{
	if (State != EStageSpotlightState::Aiming) return;
	ReleaseAimOperator();
	bAimCommittedForCue = Target.IsValid();
	SetState(Target.IsValid() ? EStageSpotlightState::Ready : EStageSpotlightState::Idle);
	CheckTarget();
}

void AStageSpotlightControl::ReleaseAimOperator()
{
	if (const AActor* Operator = AimOperator.Get())
		if (auto* Interaction = Operator->FindComponentByClass<UInteractionComponent>())
			ControlLever->ReleaseHandGrip(*Interaction);
	AimOperator.Reset();
}

void AStageSpotlightControl::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLightIntensity(DeltaSeconds);
	if (State == EStageSpotlightState::Aiming)
	{
		const auto* Pawn = Cast<APawn>(AimOperator.Get());
		auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		const auto* Interaction = Pawn ? Pawn->FindComponentByClass<UInteractionComponent>() : nullptr;
		if (!PC || !Interaction || Interaction->GetInteractionCandidate() != this)
		{
			CommitAim();
			return;
		}
		FVector2D MouseDelta = FVector2D::ZeroVector;
		const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
		const FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient ? LocalPlayer->ViewportClient->Viewport : nullptr;
		const bool bUseCursor = PC->bShowMouseCursor || (Viewport && !Viewport->HasMouseCapture());
		FVector2D Cursor;
		if (bUseCursor && PC->GetMousePosition(Cursor.X, Cursor.Y))
		{
			if (LastCursorPosition.IsSet())
			{
				const FVector2D Previous = LastCursorPosition.GetValue();
				MouseDelta = FVector2D(Cursor.X - Previous.X, Previous.Y - Cursor.Y);
			}
			LastCursorPosition = Cursor;
		}
		else
		{
			LastCursorPosition.Reset();
			PC->GetInputMouseDelta(MouseDelta.X, MouseDelta.Y);
		}
		const FVector2D Stick(PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX), -PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY));
		const FVector2D Input = MouseDelta * MouseDegreesPerUnit + Stick * StickDegreesPerSecond * DeltaSeconds;
		if (Input.IsNearlyZero()) return;

		// Screen up points away from the camera across the stage floor.
		FVector ViewLocation;
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FRotator CameraHeading(0.0, ViewRotation.Yaw, 0.0);
		const FVector Right = CameraHeading.RotateVector(FVector::RightVector);
		const FVector Away = CameraHeading.Vector();
		const FVector WorldDirection = Spotlight->GetForwardVector() + (Right * Input.X + Away * Input.Y) * FMath::DegreesToRadians(1.0);
		const FRotator RelativeAim = Spotlight->GetAttachParent()->GetComponentQuat().UnrotateVector(WorldDirection).Rotation();
		AimOffset.X = FMath::FindDeltaAngleDegrees(BaseRotation.Yaw, RelativeAim.Yaw);
		AimOffset.Y = FMath::FindDeltaAngleDegrees(BaseRotation.Pitch, RelativeAim.Pitch);
		AimOffset.X = FMath::Clamp(AimOffset.X, FMath::Min(YawLimits.X, YawLimits.Y), FMath::Max(YawLimits.X, YawLimits.Y));
		AimOffset.Y = FMath::Clamp(AimOffset.Y, FMath::Min(PitchLimits.X, PitchLimits.Y), FMath::Max(PitchLimits.X, PitchLimits.Y));
		Spotlight->SetRelativeRotation(BaseRotation + FRotator(AimOffset.Y, AimOffset.X, 0));
		const double TiltForward = FMath::Clamp(AimOffset.Y * 0.75, -32.0, 32.0);
		const double TiltSide = FMath::Clamp(AimOffset.X * 0.5, -32.0, 32.0);
		ControlLever->SetTilt(FRotator(TiltForward, 0.0, TiltSide).Quaternion());
		return;
	}
	CheckTarget();
}

void AStageSpotlightControl::UpdateLightIntensity(float DeltaSeconds)
{
	constexpr float FadeInDuration = 0.25f;
	constexpr float FadeOutDuration = 0.45f;
	constexpr float AimingFadeDuration = 0.25f;
	const bool bShouldLight = State == EStageSpotlightState::Aiming || (bAimCommittedForCue && Target.IsValid())
	    || GetWorld()->GetTimeSeconds() < KeepLightUntil;
	LightAlpha = FMath::FInterpConstantTo(LightAlpha, bShouldLight ? 1.f : 0.f, DeltaSeconds,
	    1.f / (bShouldLight ? FadeInDuration : FadeOutDuration));

	const float TargetAlpha = State == EStageSpotlightState::Aiming ? 1.f : 0.f;
	AimingIntensityAlpha = FMath::FInterpConstantTo(AimingIntensityAlpha, TargetAlpha, DeltaSeconds, 1.f / AimingFadeDuration);
	const float Multiplier = FMath::Lerp(1.f, AimingIntensityMultiplier, FMath::SmoothStep(0.f, 1.f, AimingIntensityAlpha));
	const float Intensity = BaseIntensity * FMath::SmoothStep(0.f, 1.f, LightAlpha) * Multiplier;
	if (Spotlight->Intensity != Intensity) Spotlight->SetIntensity(Intensity);
}

void AStageSpotlightControl::CheckTarget()
{
	if (!bAimCommittedForCue || State == EStageSpotlightState::Idle || State == EStageSpotlightState::Aiming) return;
	const AActor* Focus = Target.Get();
	bool bLit = false;
	if (Focus && Spotlight->IsVisible() && Spotlight->Intensity > 0.f)
	{
		const FVector Origin = Spotlight->GetComponentLocation();
		const FVector Delta = Focus->GetActorLocation() + TargetOffset - Origin;
		const float Grace = State == EStageSpotlightState::TargetLit ? TargetGraceAngle : 0.f;
		const double MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Spotlight->OuterConeAngle + Grace, 0.f, 89.f)));
		if (Delta.SizeSquared() <= FMath::Square(Spotlight->AttenuationRadius) && FVector::DotProduct(Spotlight->GetForwardVector(), Delta.GetSafeNormal()) >= MinDot)
		{
			FCollisionQueryParams Query(SCENE_QUERY_STAT(StageSpotlight), false, this);
			Query.AddIgnoredActor(Focus);
			bLit = !GetWorld()->LineTraceTestByChannel(Origin, Origin + Delta, ECC_Visibility, Query);
		}
	}
	if (bLit) SetState(EStageSpotlightState::TargetLit);
	else if (State == EStageSpotlightState::TargetLit) SetState(EStageSpotlightState::TargetLost);
}

void AStageSpotlightControl::SetState(EStageSpotlightState Value)
{
	if (State == Value) return;
	State = Value;
	OnStateChanged.Broadcast(CueId, Target.Get(), State);
}
