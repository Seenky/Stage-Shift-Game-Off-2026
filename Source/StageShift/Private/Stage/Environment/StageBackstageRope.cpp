#include "Stage/Environment/StageBackstageRope.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/Physics/DollPhysicsComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/StageShiftCharacter.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Physics/PhysicsManipulationComponent.h"
#include "UI/StageCueWidget.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float GripHeight = 160.0f;
	constexpr float TravelHeight = 1800.0f;
	constexpr float DropHeight = 1300.0f;
	constexpr float LandingOffset = 55.0f;
	constexpr float PullEnd = 0.38f;
	constexpr float TravelSpeedScale = 1.15f;
	constexpr float LiftEnd = PullEnd + 0.57f / TravelSpeedScale;
	constexpr float DropStart = LiftEnd + 0.05f;
}

AStageBackstageRope::AStageBackstageRope()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Approach = CreateDefaultSubobject<UBoxComponent>(TEXT("Approach"));
	Approach->SetupAttachment(GetRootComponent());
	Approach->SetRelativeLocation(FVector(45, 0, 110));
	Approach->InitBoxExtent(FVector(105, 115, 115));
	Approach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Approach->SetCollisionResponseToAllChannels(ECR_Ignore);
	Approach->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Approach->SetCanEverAffectNavigation(false);

	UseTarget = CreateDefaultSubobject<UBoxComponent>(TEXT("UseTarget"));
	UseTarget->SetupAttachment(GetRootComponent());
	UseTarget->SetRelativeLocation(FVector(0, 0, GripHeight));
	UseTarget->InitBoxExtent(FVector(15, 22, 45));
	UseTarget->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UseTarget->SetCollisionResponseToAllChannels(ECR_Ignore);
	UseTarget->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	UseTarget->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Linen(
	    TEXT("/Game/LVL/Game/CrimeAndPunishment/Theatre/Materials/Fabric/MI_CP_Linen_Pillow.MI_CP_Linen_Pillow"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(
	    TEXT("/Game/LVL/Game/CrimeAndPunishment/Blocking/Materials/MI_CAP_Wood.MI_CAP_Wood"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> MechanismIcon(
	    TEXT("/Game/DEV/UI/StageSignals/Icons/T_Cue_Mechanism.T_Cue_Mechanism"));
	Icon = MechanismIcon.Object;

	Grip = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Grip"));
	Grip->SetupAttachment(GetRootComponent());
	Grip->SetStaticMesh(Sphere.Object);
	Grip->SetMaterial(0, Wood.Object);
	Grip->SetRelativeScale3D(FVector(0.18, 0.18, 0.31));
	Grip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Grip->SetCanEverAffectNavigation(false);

	for (int32 Index = 0; Index < 5; ++Index)
	{
		auto* Section = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("RopeSection%d"), Index + 1));
		Section->SetupAttachment(GetRootComponent());
		Section->SetStaticMesh(Cylinder.Object);
		Section->SetMaterial(0, Linen.Object);
		Section->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Section->SetCanEverAffectNavigation(false);
		RopeSections.Add(Section);
	}

	Signal = CreateDefaultSubobject<UWidgetComponent>(TEXT("Signal"));
	Signal->SetupAttachment(GetRootComponent());
	Signal->SetRelativeLocation(FVector(0, 0, 250));
	Signal->SetWidgetSpace(EWidgetSpace::Screen);
	Signal->SetDrawSize(FVector2D(64, 64));
	Signal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Signal->SetVisibility(false);
}

void AStageBackstageRope::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetGripOffset(FVector::ZeroVector);
}

void AStageBackstageRope::BeginPlay()
{
	Super::BeginPlay();
	const TSoftClassPtr<UStageCueSignalWidget> WidgetClass(FSoftObjectPath(TEXT("/Game/DEV/UI/StageSignals/W_StageCueSignal.W_StageCueSignal_C")));
	Signal->SetWidgetClass(WidgetClass.LoadSynchronous());
	Approach->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::OnApproach);
	Approach->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::OnLeave);
	UpdateSignal();
}

void AStageBackstageRope::EndPlay(EEndPlayReason::Type Reason)
{
	if (IsActorTickEnabled()) FinishTrip(false);
	Super::EndPlay(Reason);
}

AStageBackstageRope* AStageBackstageRope::GetPartner() const
{
	return IsValid(Partner) && Partner != this && Partner->Partner == this && !Partner->IsActorBeingDestroyed() ? Partner.Get() : nullptr;
}

bool AStageBackstageRope::IsAvailable() const
{
	const AStageBackstageRope* Destination = GetPartner();
	return Destination && !IsActorTickEnabled() && !Destination->IsActorTickEnabled();
}

bool AStageBackstageRope::CanInteract_Implementation(const FInteractionContext& Context) const
{
	const auto* Player = Cast<AStageShiftCharacter>(Context.Interactor);
	if (!Player || !Player->IsPlayerControlled() || !IsAvailable() || Player->IsPhysicsMontageActive()
	    || !Player->GetCharacterMovement()->IsMovingOnGround() || !Approach->IsOverlappingActor(Player)) return false;
	if (Context.Hit.GetComponent() && Context.Hit.GetComponent() != UseTarget) return false;
	const auto* Manipulation = Player->FindComponentByClass<UPhysicsManipulationComponent>();
	return Manipulation && (!Manipulation->GetManipulatedComponent() || Manipulation->IsCarrying());
}

const UMeshComponent* AStageBackstageRope::GetInteractionMesh() const
{
	return Grip;
}

bool AStageBackstageRope::FindLanding(const AStageShiftCharacter& Player, FVector& Location) const
{
	const UCapsuleComponent* Capsule = Player.GetCapsuleComponent();
	const FVector Stand = GetActorLocation() + GetActorForwardVector().GetSafeNormal2D() * LandingOffset
	    + FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(BackstageRopeLanding), false, &Player);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(Hit, Stand + FVector::UpVector * 65.0, Stand - FVector::UpVector * 45.0, FQuat::Identity,
	        Capsule->GetCollisionObjectType(), Capsule->GetCollisionShape(), Query, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()))
	    || Hit.bStartPenetrating || !Player.GetCharacterMovement()->IsWalkable(Hit)) return false;
	Location = Hit.Location + FVector::UpVector * 6.0;
	return true;
}

EInteractionResult AStageBackstageRope::OnInteraction_Implementation(const FInteractionContext& Context)
{
	if (Context.InteractionPhase != EInteractionPhase::Start || !CanInteract_Implementation(Context)) return EInteractionResult::Rejected;
	auto* Player = CastChecked<AStageShiftCharacter>(Context.Interactor);
	auto* Interaction = Player->FindComponentByClass<UInteractionComponent>();
	FVector Landing;
	if (!Interaction || !GetPartner()->FindLanding(*Player, Landing)) return EInteractionResult::Rejected;

	Traveller = Player;
	if (const auto* Held = Player->FindComponentByClass<UPhysicsManipulationComponent>()->GetManipulatedComponent())
		HeldProp = Held->GetOwner();
	bPlayerWasHidden = Player->IsHidden();
	bPropWasHidden = HeldProp.IsValid() && HeldProp->IsHidden();
	Origin = Player->GetActorLocation();
	PullDirection = (Origin - GetActorLocation()).GetSafeNormal2D();
	if (PullDirection.IsNearlyZero()) PullDirection = GetActorForwardVector().GetSafeNormal2D();
	Player->SetActorRotation((-PullDirection).Rotation());
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->ClearAccumulatedForces();
	Movement->DisableMovement();
	if (UDollPhysicsComponent* Doll = Player->GetDollPhysics())
	{
		bSavedDollPhysics = Doll->bEnabled;
		Doll->bEnabled = false;
	}
	Interaction->PlayHandMotion(EInteractionHandMotion::RopeGrip, Grip->GetComponentLocation(), Grip);
	StartedAt = GetWorld()->GetTimeSeconds();
	SetActorTickEnabled(true);
	UpdateSignal();
	GetPartner()->UpdateSignal();
	return EInteractionResult::Completed;
}

void AStageBackstageRope::SetGripOffset(const FVector& Offset)
{
	const FVector Top = GetActorLocation() + FVector::UpVector * (TravelHeight + 330.0f);
	const FVector Bottom = GetActorLocation() + FVector::UpVector * GripHeight + Offset;
	Grip->SetWorldLocation(Bottom);
	const FVector Bow(Offset.X * 0.46, Offset.Y * 0.46, 0.0);
	FVector Start = Top;
	for (int32 Index = 0; Index < RopeSections.Num(); ++Index)
	{
		const double Alpha = static_cast<double>(Index + 1) / RopeSections.Num();
		const FVector End = FMath::Lerp(Top, Bottom, Alpha) + Bow * FMath::Sin(UE_DOUBLE_PI * Alpha);
		const FVector Segment = End - Start;
		RopeSections[Index]->SetWorldLocationAndRotation((Start + End) * 0.5, FRotationMatrix::MakeFromZ(Segment).Rotator());
		RopeSections[Index]->SetWorldScale3D(FVector(0.075, 0.075, (Segment.Size() + 1) / 100.0));
		Start = End;
	}
}

void AStageBackstageRope::SetTravellerHidden(bool bHide)
{
	if (bTravellerHidden == bHide) return;
	bTravellerHidden = bHide;
	if (auto* Player = Traveller.Get()) Player->SetActorHiddenInGame(bHide || bPlayerWasHidden);
	if (auto* Prop = HeldProp.Get()) Prop->SetActorHiddenInGame(bHide || bPropWasHidden);
}

void AStageBackstageRope::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	auto* Player = Traveller.Get();
	auto* Destination = GetPartner();
	if (!Player || !Destination || !Player->IsPlayerControlled() || Player->IsPhysicsMontageActive())
	{
		FinishTrip(false);
		return;
	}

	const float Time = static_cast<float>(GetWorld()->GetTimeSeconds() - StartedAt);
	const FVector Tension = PullDirection * 24.0 - FVector::UpVector * 26.0;
	if (Time < PullEnd)
	{
		SetGripOffset(Tension * FMath::SmoothStep(0.16f, PullEnd, Time));
		return;
	}
	if (Time >= DropStart)
	{
		SetTravellerHidden(true);
		FinishTrip(DropTraveller(*Player, *Destination));
		return;
	}

	if (Time >= LiftEnd)
	{
		SetTravellerHidden(true);
		const float Alpha = FMath::SmoothStep(LiftEnd, DropStart, Time);
		SetGripOffset(FVector::UpVector * (TravelHeight * (1.0f - Alpha)));
		return;
	}

	const float Alpha = FMath::Clamp((Time - PullEnd) / (LiftEnd - PullEnd), 0.0f, 1.0f);
	const FVector Rise = FVector::UpVector * (TravelHeight * FMath::Pow(Alpha, 1.65f));
	SetGripOffset(Rise + Tension * (1.0f - Alpha));
	FHitResult Obstacle;
	Player->SetActorLocation(Origin + Rise, true, &Obstacle, ETeleportType::TeleportPhysics);
	if (Obstacle.bBlockingHit) FinishTrip(false);
}

bool AStageBackstageRope::DropTraveller(AStageShiftCharacter& Player, const AStageBackstageRope& Destination)
{
	auto* Physics = Player.FindComponentByClass<UPhysicalMontageComponent>();
	const FVector Forward = Destination.GetActorForwardVector().GetSafeNormal2D();
	FVector Landing;
	if (!Physics || !Destination.FindLanding(Player, Landing)
	    || !Player.TeleportTo(Landing + FVector::UpVector * DropHeight, Forward.Rotation())) return false;

	FPhysicalMontageSettings Settings;
	Settings.bAllowWithoutAnimation = true;
	Settings.bApplyLaunchVelocity = false;
	Settings.TimeToRecovery = 1.17f / TravelSpeedScale;
	Settings.RecoveryPlayRate = 2.70f;

	// Shorten the flight to the same degree as the lift while keeping normal world gravity.
	constexpr double BaseDropSpeed = 770.0;
	const double Gravity = -GetWorld()->GetGravityZ();
	const double BaseFallTime = 2.0 * DropHeight / (BaseDropSpeed + FMath::Sqrt(FMath::Square(BaseDropSpeed) + 2.0 * Gravity * DropHeight));
	const double FallTime = BaseFallTime / TravelSpeedScale;
	const double DropSpeed = DropHeight / FallTime - 0.5 * Gravity * FallTime;

	FPhysicalAnimationInput Input;
	Input.InitialVelocity = FVector::DownVector * DropSpeed + Forward * 35.0;
	Input.InitialPoseRotation = FRotator(-90.0, 0.0, 0.0);
	return Physics->PlayPhysicsAnimation(Settings, Input) && Physics->IsSimulatingPhysicsMontage();
}

void AStageBackstageRope::FinishTrip(bool bDropped)
{
	auto* Destination = GetPartner();
	if (auto* Player = Traveller.Get())
	{
		SetTravellerHidden(false);
		if (auto* Interaction = Player->FindComponentByClass<UInteractionComponent>())
			Interaction->ReleaseHandGrip(*Grip);
		if (UDollPhysicsComponent* Doll = Player->GetDollPhysics()) Doll->bEnabled = bSavedDollPhysics;
		if (!bDropped && !Player->IsActorBeingDestroyed() && !Player->IsPhysicsMontageActive())
		{
			Player->SetActorLocation(Origin, false, nullptr, ETeleportType::TeleportPhysics);
			auto* Movement = Player->GetCharacterMovement();
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
	SetTravellerHidden(false);
	Traveller.Reset();
	HeldProp.Reset();
	SetActorTickEnabled(false);
	SetGripOffset(FVector::ZeroVector);
	UpdateSignal();
	if (Destination) Destination->UpdateSignal();
}

void AStageBackstageRope::UpdateSignal()
{
	TArray<AActor*> Overlapping;
	Approach->GetOverlappingActors(Overlapping, AStageShiftCharacter::StaticClass());
	const bool bShow = Icon && IsAvailable() && Overlapping.ContainsByPredicate([](const AActor* Actor)
	{
		const auto* Pawn = Cast<APawn>(Actor);
		return Pawn && Pawn->IsPlayerControlled() && Pawn->IsLocallyControlled();
	});
	Signal->SetVisibility(bShow);
	if (!bShow) return;
	Signal->InitWidget();
	if (auto* Widget = Cast<UStageCueSignalWidget>(Signal->GetUserWidgetObject())) Widget->UpdateSignal(Icon, 1.0f, false, 0.0f);
}

void AStageBackstageRope::OnApproach(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	UpdateSignal();
}

void AStageBackstageRope::OnLeave(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32)
{
	UpdateSignal();
}
