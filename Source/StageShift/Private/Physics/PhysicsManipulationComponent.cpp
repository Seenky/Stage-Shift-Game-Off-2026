// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/Interaction/ThrowAimComponent.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Core/StageShiftCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/Physics/DollPhysicsComponent.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Physics/PhysicsActor.h"

namespace
{
	constexpr double ThrowTraceSeconds = 2.5;
}

UPhysicsManipulationComponent::UPhysicsManipulationComponent()
{
	bAutoActivate = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPhysicsManipulationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner())) AddTickPrerequisiteComponent(Character->GetCharacterMovement());
	bPropPlayEnded = false;
	if (auto* Physics = GetOwner()->FindComponentByClass<UPhysicalMontageComponent>())
	{
		Physics->OnPhysAnimStarted.AddUniqueDynamic(this, &ThisClass::UpdateStagePropReadiness);
		Physics->OnRecoveryEnded.AddUniqueDynamic(this, &ThisClass::UpdateStagePropReadiness);
	}
	UpdateStagePropReadiness();
}

void UPhysicsManipulationComponent::SetCarryPoint(USceneComponent* NewCarryPoint)
{
	CarryPoint = NewCarryPoint;
	UpdateStagePropReadiness();
}

bool UPhysicsManipulationComponent::CanReceiveStageProp() const
{
	const UWorld* World = GetWorld();
	return World && CanReceiveStagePropAt(World->GetTimeSeconds());
}

bool UPhysicsManipulationComponent::CanReceiveStagePropAt(double AcceptedAt) const
{
	const AActor* Owner = GetOwner();
	if (!bAcceptStageProps || bPropPlayEnded || !IsValid(Owner) || Owner->IsActorBeingDestroyed() || !HasBegunPlay() || !CarryPoint.IsValid()
	    || CarryPoint->GetOwner() != Owner || ManipulatedComponent.IsValid() || GetGrabbedComponent()) return false;
	const auto* Member = Owner->FindComponentByClass<UStageMemberComponent>();
	if (Member && !Member->IsWaitingForStageProp(AcceptedAt)) return false;
	const auto* Physics = Owner->FindComponentByClass<UPhysicalMontageComponent>();
	return !Physics || !Physics->IsPhysicsMontageActive();
}

void UPhysicsManipulationComponent::SetAcceptStageProps(bool bAccept)
{
	bAcceptStageProps = bAccept;
	UpdateStagePropReadiness();
}

void UPhysicsManipulationComponent::UpdateStagePropReadiness()
{
	const bool bReady = CanReceiveStageProp();
	if (bLastPropReady == bReady) return;
	bLastPropReady = bReady;
	OnStagePropReadinessChanged.Broadcast(bReady);
}

bool UPhysicsManipulationComponent::CanManipulate(const FHitResult& Hit, EPhysicsInteractionType Type) const
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!IsValid(GetOwner()) || !IsValid(Component) || ManipulatedComponent.IsValid() || GetGrabbedComponent() || !Component->IsRegistered() || Component->GetWorld() != GetWorld()
	    || Component->GetOwner() == GetOwner() || !Component->IsSimulatingPhysics(Hit.BoneName))
	{
		return false;
	}

	const AStageShiftCharacter* Doll = Cast<AStageShiftCharacter>(GetOwner());
	if (Doll && Doll->IsPhysicsMontageActive()) return false;
	if (Type == EPhysicsInteractionType::Carry)
	{
		return CarryPoint.IsValid() && CarryPoint->GetOwner() == GetOwner() && Component == Component->GetOwner()->GetRootComponent();
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Type == EPhysicsInteractionType::Drag && Character && Character->GetCharacterMovement()->IsMovingOnGround();
}

bool UPhysicsManipulationComponent::BeginManipulation(const FHitResult& Hit, EPhysicsInteractionType Type)
{
	return BeginManipulationInternal(Hit, Type, true);
}

bool UPhysicsManipulationComponent::BeginManipulationInternal(const FHitResult& Hit, EPhysicsInteractionType Type, bool bPlayPickupAnimation)
{
	if (!CanManipulate(Hit, Type)) return false;
	UPrimitiveComponent* Component = Hit.GetComponent();
	ActiveType = Type;
	ManipulatedComponent = Component;
	ManipulatedActor = Component->GetOwner();

	if (Type == EPhysicsInteractionType::Carry)
	{
		const ACharacter* Character = Cast<ACharacter>(GetOwner());
		UAnimInstance* Instance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		const UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>();
		const FInteractionAnimation* Animation = Interaction ? Interaction->FindAnimationForTarget(Component->GetOwner()) : nullptr;
		UAnimMontage* Montage = Animation ? Animation->Montage.Get() : nullptr;
		const bool bHasGrabNotify = Montage
		    && Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
		{
			return Event.Notify && Event.Notify->GetNotifyName() == TEXT("GrabObject");
		});
		if (bPlayPickupAnimation && Instance && bHasGrabNotify)
		{
			LocalGripLocation = Component->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
			PickupInstance = Instance;
			PickupMontage = Montage;
			bPickupPending = true;
			Instance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &ThisClass::OnPickupNotify);
			Instance->OnMontageEnded.AddUniqueDynamic(this, &ThisClass::OnPickupMontageEnded);
			SetComponentTickEnabled(true);
		}
		else if (!AttachCarriedObject(false))
		{
			ReleaseComponent();
			return false;
		}
	}
	else
	{
		BeginDrag(Hit);
		SetComponentTickEnabled(true);
	}

	ManipulatedActor->OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnManipulatedActorEndPlay);

	UpdateStagePropReadiness();
	if (IsCarrying())
	{
		if (auto* Prop = Cast<AStagePhysicsActor>(ManipulatedActor.Get())) Prop->NotifyPropCarried(GetOwner());
		OnItemPickedUp.Broadcast(ManipulatedActor.Get());
	}

	return true;
}

bool UPhysicsManipulationComponent::TransferCarriedPropTo(UPhysicsManipulationComponent& Receiver)
{
	auto* Component = ManipulatedComponent.Get();
	auto* Prop = ManipulatedActor.Get();
	if (!IsCarrying() || !IsValid(Component) || !IsValid(Prop) || Component->IsSimulatingPhysics() || !Receiver.CanReceiveStageProp()) return false;
	if (!Component->AttachToComponent(Receiver.CarryPoint.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale)) return false;

	Receiver.ActiveType = EPhysicsInteractionType::Carry;
	Receiver.ManipulatedComponent = Component;
	Receiver.ManipulatedActor = Prop;
	Receiver.PreviousCollision = PreviousCollision;
	Receiver.LocalCarryCenter = LocalCarryCenter;
	Receiver.bWasSimulatingPhysics = bWasSimulatingPhysics;
	Receiver.bCarryAttached = true;
	Prop->OnEndPlay.AddUniqueDynamic(&Receiver, &ThisClass::OnManipulatedActorEndPlay);

	// The receiver now owns the attachment and the original collision state.
	bCarryAttached = false;
	ReleaseComponent();
	Receiver.UpdateStagePropReadiness();
	return true;
}

FTransform UPhysicsManipulationComponent::GetDragFrame() const
{
	const UPrimitiveComponent* Component = ManipulatedComponent.Get();
	if (!Component) return FTransform::Identity;
	return DragBoneName.IsNone() ? Component->GetComponentTransform() : Component->GetSocketTransform(DragBoneName);
}

void UPhysicsManipulationComponent::BeginDrag(const FHitResult& Hit)
{
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	DragCharacter = Cast<ACharacter>(GetOwner());
	DragBoneName = Hit.BoneName;
	const FBodyInstance* Body = Component->GetBodyInstance(DragBoneName);
	DragMass = FMath::Max(Body ? Body->GetBodyMass() : Component->GetMass(), 0.1f);
	SavedDragAngularDamping = Component->GetAngularDamping();
	AppliedDragAngularDamping = FMath::Max(SavedDragAngularDamping, DragAngularDamping);
	if (AppliedDragAngularDamping != SavedDragAngularDamping) Component->SetAngularDamping(AppliedDragAngularDamping);
	SetDragGrip(Hit.ImpactPoint);
	DragEffort = 0.0f;
	if (ACharacter* Character = DragCharacter.Get())
	{
		UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
		bSavedOrientToMovement = Movement->bOrientRotationToMovement;
		bSavedControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		bSavedControllerYaw = Character->bUseControllerRotationYaw;
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
		Character->bUseControllerRotationYaw = false;
		if (AStageShiftCharacter* Doll = Cast<AStageShiftCharacter>(Character))
		{
			Doll->GetDollPhysics()->SetDragging(true);
		}
	}
}

void UPhysicsManipulationComponent::SetDragGrip(const FVector& GripPoint)
{
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	const ACharacter* Character = DragCharacter.Get();
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const FVector Facing = (GripPoint - GetOwner()->GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER, GetOwner()->GetActorForwardVector());
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Facing).GetSafeNormal();
	const FTransform Frame = GetDragFrame();
	FVector ReachCenter = FVector::ZeroVector;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const FName Shoulder = Side == 0 ? FName(TEXT("upperarm_l")) : FName(TEXT("upperarm_r"));
		const FName Hand = Side == 0 ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
		const FName Socket = Side == 0 ? FName(TEXT("DragGrip_L")) : FName(TEXT("DragGrip_R"));
		const FVector ReachStart = Mesh && Mesh->GetBoneIndex(Shoulder) != INDEX_NONE ? Mesh->GetSocketLocation(Shoulder) : GetOwner()->GetActorLocation();
		ReachCenter += ReachStart * 0.5;
		FVector Point = GripPoint + Right * (DragHandSpacing * (Side == 0 ? -0.5f : 0.5f));
		if (Component->DoesSocketExist(Socket))
			Point = Component->GetSocketLocation(Socket);
		else
		{
			const FVector Probe = Point + (ReachStart - Point).GetSafeNormal(UE_SMALL_NUMBER, -Facing) * 8.0;
			FVector Surface;
			if (Component->GetClosestPointOnCollision(Probe, Surface, DragBoneName) >= 0.0f)
			{
				const FVector Normal = (Probe - Surface).GetSafeNormal(UE_SMALL_NUMBER, -Facing);
				double Clearance = 6.0;
				if (const FBodyInstance* HandBody = Mesh ? Mesh->GetBodyInstance(Hand) : nullptr)
				{
					const FBox Bounds = HandBody->GetBodyBounds();
					if (Bounds.IsValid)
					{
						const FVector Offset = Bounds.GetCenter() - Mesh->GetSocketLocation(Hand);
						Clearance = FMath::Max(Clearance, FVector::DotProduct(Bounds.GetExtent(), Normal.GetAbs()) - FVector::DotProduct(Offset, Normal) + 1.0);
					}
				}
				Point = Surface + Normal * Clearance;
			}
		}
		DragLocalHands[Side] = Frame.InverseTransformPosition(Point);
	}
	DragLocalGripNormal = Frame.InverseTransformVectorNoScale((ReachCenter - GripPoint).GetSafeNormal());
	const FVector Center = Frame.TransformPosition((DragLocalHands[0] + DragLocalHands[1]) * 0.5);
	const float Distance = FVector::Dist2D(Center, GetOwner()->GetActorLocation());
	DragTargetYaw = (Center - GetOwner()->GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER, Facing).Rotation().Yaw;
	DragRestDistance = FMath::Min(Distance, FMath::Max(DragDistance, 1.0f));
	DragReach = FMath::Max(Distance, DragRestDistance) + FMath::Max(DragReachSlack, 1.0f);
}

float UPhysicsManipulationComponent::GetDragMovementScale() const
{
	if (!IsDragging()) return 1.0f;
	// No flat speed plateau below the reference mass: 1 kg stays light, 35 kg is a load.
	const float Load = FMath::Max(DragMass - 1.0f, 0.0f) / FMath::Max(DragReferenceMass, 0.1f);
	const float BaseScale = FMath::Clamp(DragSpeedMultiplier, 0.01f, 1.0f);
	// Keep an intentional two-hand grip useful compared with nudging the prop.
	return FMath::Clamp(BaseScale / FMath::Sqrt(1.0f + Load), FMath::Clamp(DragMinimumSpeedMultiplier, 0.01f, BaseScale), BaseScale);
}

bool UPhysicsManipulationComponent::GetDragPose(FVector& LeftHand, FVector& RightHand, FVector& Facing, float& Effort) const
{
	if (!IsDragging() || !GetOwner()) return false;
	const FTransform Frame = GetDragFrame();
	LeftHand = Frame.TransformPosition(DragLocalHands[0]);
	RightHand = Frame.TransformPosition(DragLocalHands[1]);
	const FVector ToGrip = (LeftHand + RightHand) * 0.5 - GetOwner()->GetActorLocation();
	const ACharacter* Character = DragCharacter.Get();
	const float FacingRadius = Character ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 0.0f;
	// Keep facing stable while the grip passes underneath the character.
	Facing = ToGrip.SizeSquared2D() > FMath::Square(FacingRadius) ? ToGrip.GetSafeNormal2D() : GetOwner()->GetActorForwardVector();
	// A rolling prop may reverse its stored anchors. Keep each hand on its own
	// side of the character instead of asking the IK to cross the arms.
	const FVector RightDirection = FVector::CrossProduct(FVector::UpVector, Facing);
	if (FVector::DotProduct(RightHand - LeftHand, RightDirection) < 0.0) Swap(LeftHand, RightHand);
	Effort = DragEffort;
	return true;
}

FVector UPhysicsManipulationComponent::ConstrainDragMovement(const FVector& Delta) const
{
	if (!IsDragging() || !GetOwner()) return Delta;
	const FVector Center = GetDragFrame().TransformPosition((DragLocalHands[0] + DragLocalHands[1]) * 0.5);
	const FVector From = (GetOwner()->GetActorLocation() - Center) * FVector(1, 1, 0);
	const FVector Proposed = From + Delta * FVector(1, 1, 0);
	// Never teleport the player inward when the object itself moves away.
	const double Radius = FMath::Max(static_cast<double>(DragReach), From.Size());
	FVector Position = Proposed.GetClampedToMaxSize(Radius);
	const double MinimumRadius = FMath::Min(FMath::Max(DragRestDistance - FMath::Max(DragReachSlack, 1.0f), 0.0f), static_cast<float>(From.Size()));
	if (Position.SizeSquared() < FMath::Square(MinimumRadius)) Position = Position.GetSafeNormal(UE_SMALL_NUMBER, From.GetSafeNormal()) * MinimumRadius;
	const FVector Limited = Position - From;
	return FVector(Limited.X, Limited.Y, Delta.Z);
}

void UPhysicsManipulationComponent::CancelManipulation()
{
	if (UInteractionComponent* Interaction = GetOwner() ? GetOwner()->FindComponentByClass<UInteractionComponent>() : nullptr)
	{
		Interaction->CancelInteraction();
	}
	ReleaseComponent();
}

void UPhysicsManipulationComponent::UpdateDrag(float DeltaTime)
{
	ACharacter* Character = DragCharacter.Get();
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	const AStageShiftCharacter* Doll = Cast<AStageShiftCharacter>(Character);
	if (!Character || !Component || !Component->IsRegistered() || !Component->IsSimulatingPhysics(DragBoneName) || !Character->GetCharacterMovement()->IsMovingOnGround()
	    || (Doll && Doll->IsPhysicsMontageActive()))
	{
		CancelManipulation();
		return;
	}
	FVector Left, Right, Facing;
	float Effort;
	GetDragPose(Left, Right, Facing, Effort);
	FVector Center = (Left + Right) * 0.5;
	const FVector OwnerLocation = Character->GetActorLocation();
	if (FVector::Dist(OwnerLocation, Center) > FMath::Max(DragBreakDistance, DragReach + 30.0f))
	{
		CancelManipulation();
		return;
	}
	float Distance = FVector::Dist2D(OwnerLocation, Center);
	const float ReachSlack = FMath::Max(DragReachSlack, 1.0f);
	const USkeletalMeshComponent* Mesh = Character->GetMesh();
	const FVector ReachStart = Mesh && Mesh->GetBoneIndex(TEXT("upperarm_r")) != INDEX_NONE ? Mesh->GetSocketLocation(TEXT("upperarm_r")) : OwnerLocation;
	const FVector GripNormal = GetDragFrame().TransformVectorNoScale(DragLocalGripNormal);
	const bool bGripTurnedAway = FVector::DotProduct(GripNormal, (ReachStart - Center).GetSafeNormal()) < 0.25;
	if (Distance + ReachSlack < DragRestDistance || bGripTurnedAway)
	{
		FVector Surface;
		if (Component->GetClosestPointOnCollision(ReachStart, Surface, DragBoneName) >= 0.0f)
		{
			SetDragGrip(Surface);
			GetDragPose(Left, Right, Facing, Effort);
			Center = (Left + Right) * 0.5;
			Distance = FVector::Dist2D(OwnerLocation, Center);
		}
		else
		{
			DragRestDistance = Distance;
			DragReach = Distance + ReachSlack;
		}
	}
	const FVector ToGrip = (Center - OwnerLocation) * FVector(1, 1, 0);
	const UThrowAimComponent* Aim = GetOwner()->FindComponentByClass<UThrowAimComponent>();
	FVector CursorTarget;
	const bool bCursorSteering = Aim && Aim->GetAimTarget(CursorTarget);
	const float GripYaw = Facing.Rotation().Yaw;
	const float MaxTurnSpeed = FMath::Max(DragTurnSpeed, 1.0f);
	float FacingYaw = GripYaw;
	FVector Error = ToGrip.GetSafeNormal() * (DragRestDistance - Distance);
	FVector TurnVelocity = FVector::ZeroVector;
	if (bCursorSteering && DeltaTime > UE_SMALL_NUMBER)
	{
		const FVector CursorDirection = (CursorTarget - OwnerLocation).GetSafeNormal2D(UE_SMALL_NUMBER, Character->GetActorForwardVector());
		const float PreviousYaw = DragTargetYaw;
		const float Turn = FMath::FindDeltaAngleDegrees(PreviousYaw, static_cast<float>(CursorDirection.Rotation().Yaw));
		DragTargetYaw = FMath::UnwindDegrees(PreviousYaw + FMath::Clamp(Turn, -MaxTurnSpeed * DeltaTime, MaxTurnSpeed * DeltaTime));
		// Advance the grip around the character along an arc. Limit the lead when
		// a heavy/blocked prop lags, so the spring does not pull it through the torso.
		const float Lead = FMath::FindDeltaAngleDegrees(GripYaw, DragTargetYaw);
		DragTargetYaw = FMath::UnwindDegrees(GripYaw + FMath::Clamp(Lead, -45.0f, 45.0f));
		// Let the torso anticipate the turn slightly, but keep the real grip in
		// front even when the cursor is behind us or the prop cannot move yet.
		const float FacingLead = Distance > Character->GetCapsuleComponent()->GetScaledCapsuleRadius() ? FMath::Clamp(Lead, -12.0f, 12.0f) : 0.0f;
		FacingYaw = GripYaw + FacingLead;
		const FVector TargetDirection = FRotator(0.0, DragTargetYaw, 0.0).Vector();
		Error = TargetDirection * DragRestDistance - ToGrip;
		const float ActualTurn = FMath::FindDeltaAngleDegrees(PreviousYaw, DragTargetYaw);
		const float TurnRate = FMath::DegreesToRadians(FMath::Clamp(ActualTurn / DeltaTime, -MaxTurnSpeed, MaxTurnSpeed));
		TurnVelocity = FVector::CrossProduct(FVector::UpVector, TargetDirection) * (DragRestDistance * TurnRate);
	}
	else
	{
		DragTargetYaw = GripYaw;
	}
	Character->SetActorRotation(FMath::RInterpConstantTo(Character->GetActorRotation(), FRotator(0.0, FacingYaw, 0.0), DeltaTime, MaxTurnSpeed));
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector Input = Movement->GetCurrentAcceleration();
	const FVector DriveVelocity =
	    Input.IsNearlyZero() ? Character->GetVelocity() : (Input / FMath::Max(Movement->GetMaxAcceleration(), 1.0f)).GetClampedToMaxSize(1.0) * Movement->GetMaxSpeed();
	// Input keeps exerting effort when the player's reach leash blocks further movement.
	const FVector RelativeVelocity = (DriveVelocity + TurnVelocity - Component->GetPhysicsLinearVelocity(DragBoneName)) * FVector(1, 1, 0);
	// Tune all force-related settings in units of the same reference load.
	// 100 kg at a 100 kg reference has the response of 25 kg at the default 25 kg reference.
	constexpr float TuningReferenceMass = 25.0f;
	const float StrengthScale = FMath::Max(DragReferenceMass, 0.1f) / TuningReferenceMass;
	const float Stiffness = FMath::Max(DragSpringStiffness, 1.0f) * StrengthScale;
	const float Damping = 2.0f * FMath::Max(DragDampingRatio, 0.1f) * FMath::Sqrt(Stiffness * DragMass);
	const double ForceLimit = FMath::Min(FMath::Max(DragMaxForce, 1.0f) * StrengthScale * 100.0, static_cast<double>(DragMass * FMath::Max(DragMaxAcceleration, 1.0f)));
	// Implicit damping prevents a light body from oscillating when the allowed acceleration rises.
	const double Step = FMath::Max(DeltaTime, 0.0f);
	const double Stabilization = 1.0 + (Damping * Step + Stiffness * Step * Step) / DragMass;
	const FVector Force = ((Error * Stiffness + RelativeVelocity * Damping) / Stabilization).GetClampedToMaxSize(ForceLimit);
	// Force mode, not acceleration mode. No vertical spring or kinematic handle to lift the prop.
	if (!Force.IsNearlyZero(1.0)) Component->AddForce(Force, DragBoneName, false);
	DragEffort = FMath::FInterpTo(DragEffort, static_cast<float>(Force.Size() / ForceLimit), DeltaTime, 6.0f);
}

bool UPhysicsManipulationComponent::AttachCarriedObject(bool bKeepWorldTransform)
{
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	if (!Component || !CarryPoint.IsValid() || !Component->IsSimulatingPhysics()) return false;

	PreviousCollision = Component->GetCollisionEnabled();
	LocalCarryCenter = Component->GetComponentTransform().InverseTransformPosition(Component->GetCenterOfMass());
	bWasSimulatingPhysics = true;
	Component->SetSimulatePhysics(false);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FAttachmentTransformRules& Rules = bKeepWorldTransform ? FAttachmentTransformRules::KeepWorldTransform : FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	if (!Component->AttachToComponent(CarryPoint.Get(), Rules))
	{
		Component->SetCollisionEnabled(PreviousCollision);
		Component->SetSimulatePhysics(bWasSimulatingPhysics);
		return false;
	}
	bCarryAttached = true;
	if (auto* Prop = Cast<AStagePhysicsActor>(ManipulatedActor.Get())) Prop->RefreshPickupOutline();
	if (GrabSound) UGameplayStatics::PlaySoundAtLocation(this, GrabSound, Component->GetComponentLocation());
	return true;
}

bool UPhysicsManipulationComponent::GetPickupTarget(FVector& WorldLocation, float& Weight, FVector* HandOffset) const
{
	const UPrimitiveComponent* Component = ManipulatedComponent.Get();
	const UAnimInstance* Instance = PickupInstance.Get();
	if (!Component || !Instance || !Instance->Montage_IsActive(PickupMontage.Get())) return false;

	WorldLocation = bCarryAttached               ? GrabLocation
	    : Component->DoesSocketExist(GripSocket) ? Component->GetSocketLocation(GripSocket)
	                                             : Component->GetComponentTransform().TransformPosition(LocalGripLocation);
	Weight = FMath::Clamp(Instance->GetCurveValue(TEXT("PickupReach")), 0.0f, 1.0f);
	if (HandOffset && CarryPoint.IsValid())
	{
		const USkeletalMeshComponent* Mesh = Instance->GetSkelMeshComponent();
		const int32 HandIndex = Mesh ? Mesh->GetBoneIndex(TEXT("hand_r")) : INDEX_NONE;
		*HandOffset = HandIndex != INDEX_NONE ? Mesh->GetBoneTransform(HandIndex).InverseTransformPosition(CarryPoint->GetComponentLocation()) : FVector::ZeroVector;
	}
	return true;
}

void UPhysicsManipulationComponent::OnPickupNotify(FName Name, const FBranchingPointNotifyPayload& Payload)
{
	UAnimInstance* Instance = PickupInstance.Get();
	const FAnimMontageInstance* Montage = Instance ? Instance->GetActiveInstanceForMontage(PickupMontage.Get()) : nullptr;
	if (!bPickupPending || Name != TEXT("GrabObject") || !Montage || Montage->GetInstanceID() != Payload.MontageInstanceID) return;

	float Weight = 0.0f;
	GetPickupTarget(GrabLocation, Weight);
	// Selection already accepted the pickup; the reach animation must not veto it.
	if (!AttachCarriedObject(false))
	{
		if (UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
		ReleaseComponent();
		return;
	}
	bPickupPending = false;
	SetComponentTickEnabled(false);

	if (auto* Prop = Cast<AStagePhysicsActor>(ManipulatedActor.Get())) Prop->NotifyPropCarried(GetOwner());
	OnItemPickedUp.Broadcast(ManipulatedActor.Get());
}

void UPhysicsManipulationComponent::ClearPickupPlayback()
{
	if (UAnimInstance* Instance = PickupInstance.Get())
	{
		Instance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &ThisClass::OnPickupNotify);
		Instance->OnMontageEnded.RemoveDynamic(this, &ThisClass::OnPickupMontageEnded);
	}
	PickupInstance.Reset();
	PickupMontage.Reset();
}

void UPhysicsManipulationComponent::OnPickupMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	UAnimInstance* Instance = PickupInstance.Get();
	if (Montage != PickupMontage.Get() || (Instance && Instance->Montage_IsActive(Montage))) return;
	if (bPickupPending)
	{
		if (UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
		ReleaseComponent();
	}
	ClearPickupPlayback();
}

void UPhysicsManipulationComponent::EndManipulation(float HoldDuration, bool bCancelled)
{
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	const bool bCarry = bCarryAttached;
	const bool bThrow = Component && bCarry && !bCancelled && HoldDuration > TapDuration && IsValid(GetOwner()) && GetWorld();
	const FThrowAim Aim = ThrowAim;
	const FVector Center = bThrow ? Component->GetComponentTransform().TransformPosition(LocalCarryCenter) : FVector::ZeroVector;
	const FVector Adjustment = bThrow ? GetThrowReleaseAdjustment(*Component, Center) : FVector::ZeroVector;
	const FVector Velocity = bThrow ? ResolveThrowVelocity(*Component, Aim, Center, Center + Adjustment, HoldDuration) : FVector::ZeroVector;

	ReleaseComponent();

	if (IsValid(Component) && bCarry && Component->IsSimulatingPhysics() && IsValid(GetOwner()))
	{
		if (USoundBase* Sound = bThrow ? ThrowSound.Get() : (bCancelled ? nullptr : ReleaseSound.Get()))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Component->GetComponentLocation());
		}
		Component->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Component->SetPhysicsLinearVelocity(FVector::ZeroVector);
		if (bThrow)
		{
			if (!Adjustment.IsNearlyZero())
				Component->SetWorldLocation(Component->GetComponentLocation() + Adjustment, false, nullptr, ETeleportType::TeleportPhysics);
			Component->SetPhysicsLinearVelocity(Velocity);
			if (auto* Prop = Cast<AStagePhysicsActor>(Component->GetOwner())) Prop->WatchThrownProp(GetOwner());
		}
	}
}

void UPhysicsManipulationComponent::SetThrowTarget(const FVector& WorldLocation, const FVector& SurfaceNormal)
{
	ThrowAim.bHasTarget = !WorldLocation.ContainsNaN();
	ThrowAim.Target = WorldLocation;
	ThrowAim.SurfaceNormal = SurfaceNormal.ContainsNaN() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
	ThrowAim.ViewDirection = FVector::ZeroVector;
}

void UPhysicsManipulationComponent::SetThrowTargetFromView(const FVector& WorldLocation, const FVector& SurfaceNormal, const FVector& ViewDirection)
{
	SetThrowTarget(WorldLocation, SurfaceNormal);
	ThrowAim.ViewDirection = ViewDirection.GetSafeNormal();
}

void UPhysicsManipulationComponent::AddThrowIgnoredCharacters(const UWorld& World, FCollisionQueryParams& Query)
{
	for (TActorIterator<ACharacter> It(&World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
}

FVector UPhysicsManipulationComponent::CalculateThrowVelocity(
    const UPrimitiveComponent& Component, const FThrowAim& Aim, const FVector& Center, const FVector& Start, float HoldDuration, double SweepRadius,
    FVector& OutTarget, float& OutLobSpeedLimit) const
{
	const float Charge = FMath::Clamp((HoldDuration - TapDuration) / FMath::Max(MaxChargeDuration, UE_SMALL_NUMBER), 0.0f, 1.0f);
	const float Speed = FMath::Lerp(MinThrowSpeed, FMath::Max(MinThrowSpeed, MaxThrowSpeed), Charge);
	OutLobSpeedLimit = Speed;
	OutTarget = Aim.Target;
	if (!Aim.bHasTarget)
	{
		const FVector Forward = Aim.Direction.IsNearlyZero() ? GetOwner()->GetActorForwardVector() : Aim.Direction;
		return GetOwner()->GetVelocity() + (Forward + FVector::UpVector * ThrowUpwardBias).GetSafeNormal() * Speed;
	}

	if (Speed <= UE_SMALL_NUMBER) return FVector::ZeroVector;
	const bool bPlayerAim = !Aim.ViewDirection.IsNearlyZero();
	double SurfaceOffset = SweepRadius;
	if (!bPlayerAim)
	{
		const FVector CenterOffset = Component.Bounds.Origin - Center;
		SurfaceOffset = FMath::Max(
		    FVector::DotProduct(Component.Bounds.BoxExtent, Aim.SurfaceNormal.GetAbs()) - FVector::DotProduct(CenterOffset, Aim.SurfaceNormal), 0.0);
	}
	OutTarget = Aim.Target + Aim.SurfaceNormal * SurfaceOffset;
	const FVector Offset = OutTarget - Start;
	const float GravityZ = Component.IsGravityEnabled() && GetWorld() ? GetWorld()->GetGravityZ() : 0.0f;
	if (FMath::Abs(GravityZ) > UE_SMALL_NUMBER && Offset.SizeSquared2D() > 1.0)
	{
		float AimSpeed = Speed;
		if (bAssistTargetRange)
		{
			const float MaxSpeed = bPlayerAim ? FMath::Max(MaxThrowSpeed, MaxAimAssistThrowSpeed) : MaxThrowSpeed;
			// A low charge can be below the minimum ballistic speed for this point.
			const double RequiredSquared = FMath::Abs(GravityZ) * FMath::Max(Offset.Z + Offset.Size(), 0.0);
			const double MinimumSpeed = FMath::Sqrt(RequiredSquared) * (1.0 + UE_KINDA_SMALL_NUMBER);
			AimSpeed = FMath::Max(Speed, FMath::Min(static_cast<float>(MinimumSpeed), MaxSpeed));
			OutLobSpeedLimit = FMath::Max(AimSpeed, MaxSpeed);
		}
		UGameplayStatics::FSuggestProjectileVelocityParameters Parameters(this, Start, OutTarget, AimSpeed);
		Parameters.OverrideGravityZ = GravityZ;
		Parameters.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;
		Parameters.bAcceptClosestOnNoSolutions = true;
		FVector Velocity;
		if (UGameplayStatics::SuggestProjectileVelocity(Parameters, Velocity) && !Velocity.ContainsNaN()) return Velocity;
	}
	return Offset.IsNearlyZero() ? GetOwner()->GetActorForwardVector() * Speed : Offset.GetSafeNormal() * Speed;
}

FVector UPhysicsManipulationComponent::ResolveThrowVelocity(UPrimitiveComponent& Component, const FThrowAim& Aim, const FVector& Center,
    const FVector& Start, float HoldDuration, TArray<FVector>* Path, FHitResult* OutHit, bool bDrawDebug, bool* bOutBlocked, bool* bOutUsesHighArc) const
{
	if (bOutUsesHighArc) *bOutUsesHighArc = false;
	const double BoundsRadius = Component.CalcBounds(FTransform::Identity).SphereRadius * Component.GetComponentTransform().GetScale3D().GetAbsMax();
	const double SweepRadius = FMath::Max(BoundsRadius / 1.5, 1.0);
	FVector Target;
	float LobSpeedLimit;
	FVector Velocity = CalculateThrowVelocity(Component, Aim, Center, Start, HoldDuration, SweepRadius, Target, LobSpeedLimit);
	const double Gravity = Component.IsGravityEnabled() ? -GetWorld()->GetGravityZ() : 0.0;
	const bool bPlayerAim = Aim.bHasTarget && !Aim.ViewDirection.IsNearlyZero();
	const bool bCanLob = bPlayerAim && MaxThrowLobHeight > 0.f && Gravity > UE_SMALL_NUMBER;
	if (!Path && !bCanLob) return Velocity;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(ThrowPreview), false, GetOwner());
	Query.AddIgnoredActor(Component.GetOwner());
	Query.bFindInitialOverlaps = false;
	AddThrowIgnoredCharacters(*GetWorld(), Query);

	const auto IsLanding = [&Aim](const FHitResult& Hit)
	{
		return Hit.bBlockingHit && Hit.ImpactNormal.Z >= 0.7
		    && Hit.ImpactPoint.Z <= Aim.Target.Z + 35.0;
	};
	const double LandingTolerance = FMath::Max(25.0, BoundsRadius + 15.0);
	const auto IsLandingAtTarget = [&](const FHitResult& Hit)
	{
		return IsLanding(Hit) && FVector::DistSquared2D(Hit.ImpactPoint, Aim.Target) <= FMath::Square(LandingTolerance);
	};
	FHitResult Hit;
	const FVector Obstruction = TraceThrowPath(Component, Start, Velocity, SweepRadius, Query, Path, Hit, false);
	if (bCanLob && Hit.bBlockingHit && !IsLanding(Hit))
	{
		// Center the prop over the floor target so it can clear the far side of the obstacle.
		const FVector Offset = Target - Start;
		const FVector2D Horizontal(Offset.X, Offset.Y);
		const FVector2D ObstructionHorizontal(Obstruction.X - Start.X, Obstruction.Y - Start.Y);
		const double Progress = Horizontal.SizeSquared() > 1.0 ? FVector2D::DotProduct(ObstructionHorizontal, Horizontal) / Horizontal.SizeSquared() : 0.0;
		if (Progress > UE_KINDA_SMALL_NUMBER && Progress < 1.0 - UE_KINDA_SMALL_NUMBER)
		{
			// At horizontal progress u, gravity adds g*t²*u*(1-u)/2 above the straight start-to-target line.
			const double HeightAtObstruction = Obstruction.Z - Start.Z - Offset.Z * Progress;
			const double HeightPerTimeSquared = 0.5 * Gravity * Progress * (1.0 - Progress);
			constexpr double HeightStep = 20.0;
			TArray<FVector> LobPath;
			for (double Rise = HeightStep; Rise <= MaxThrowLobHeight; Rise += HeightStep)
			{
				const double TimeSquared = (HeightAtObstruction + Rise) / HeightPerTimeSquared;
				if (TimeSquared <= 0.0) continue;
				if (TimeSquared > FMath::Square(ThrowTraceSeconds)) break;
				const double FlightTime = FMath::Sqrt(TimeSquared);
				const FVector LobVelocity = Offset / FlightTime + FVector(0.0, 0.0, 0.5 * Gravity * FlightTime);
				if (LobVelocity.SizeSquared() > FMath::Square(static_cast<double>(LobSpeedLimit))) continue;

				FHitResult LobHit;
				TraceThrowPath(Component, Start, LobVelocity, SweepRadius, Query, Path ? &LobPath : nullptr, LobHit, false);
				if (!IsLandingAtTarget(LobHit)) continue;

				if (bOutUsesHighArc) *bOutUsesHighArc = true;
				Velocity = LobVelocity;
				Hit = LobHit;
				if (Path) *Path = MoveTemp(LobPath);
				break;
			}
		}
	}
	if (bDrawDebug) TraceThrowPath(Component, Start, Velocity, SweepRadius, Query, nullptr, Hit, true);
	if (OutHit) *OutHit = Hit;
	if (bOutBlocked) *bOutBlocked = bPlayerAim ? !IsLandingAtTarget(Hit) : Hit.bBlockingHit && Hit.ImpactNormal.Z < 0.7;
	return Velocity;
}

bool UPhysicsManipulationComponent::PredictThrow(float HoldDuration, TArray<FVector>& Path, FHitResult& Hit, bool bDrawDebug,
    bool* bOutBlocked, bool* bOutUsesHighArc) const
{
	UPrimitiveComponent* Component = ManipulatedComponent.Get();
	if (!Component || !bCarryAttached || !bWasSimulatingPhysics || HoldDuration <= TapDuration || !GetWorld()) return false;

	const FVector Center = Component->GetComponentTransform().TransformPosition(LocalCarryCenter);
	const FVector Start = Center + GetThrowReleaseAdjustment(*Component, Center);
	ResolveThrowVelocity(*Component, ThrowAim, Center, Start, HoldDuration, &Path, &Hit, bDrawDebug, bOutBlocked, bOutUsesHighArc);
	return Path.Num() > 1;
}

FVector UPhysicsManipulationComponent::TraceThrowPath(const UPrimitiveComponent& Component, const FVector& Start, const FVector& Velocity,
	double SweepRadius, const FCollisionQueryParams& Query, TArray<FVector>* Path, FHitResult& Hit, bool bDrawDebug) const
{
	const FCollisionShape Shape = FCollisionShape::MakeSphere(SweepRadius);
	const FCollisionResponseParams Responses(Component.GetCollisionResponseToChannels());
	const double Gravity = Component.IsGravityEnabled() ? GetWorld()->GetGravityZ() : 0.0;

	constexpr int32 StepsPerSecond = 24;
	constexpr int32 StepCount = static_cast<int32>(ThrowTraceSeconds * StepsPerSecond);
	if (Path)
	{
		Path->Reset(StepCount + 1);
		Path->Add(Start);
	}
	constexpr float DebugLifetime = 0.075f;
	if (bDrawDebug) DrawDebugSphere(GetWorld(), Start, SweepRadius, 12, FColor::Orange, false, DebugLifetime);
	FVector Previous = Start;
	for (int32 Step = 1; Step <= StepCount; ++Step)
	{
		const double Time = Step / static_cast<double>(StepsPerSecond);
		const FVector Next = Start + Velocity * Time + FVector(0, 0, 0.5 * Gravity * Time * Time);
		const bool bHit = GetWorld()->SweepSingleByChannel(
		    Hit, Previous, Next, FQuat::Identity, Component.GetCollisionObjectType(), Shape, Query, Responses);
		if (bDrawDebug)
		{
			DrawDebugLine(GetWorld(), Previous, bHit ? Hit.Location : Next, FColor::Orange, false, DebugLifetime, 0, 1.5f);
			if (bHit)
			{
				DrawDebugLine(GetWorld(), Hit.Location, Next, FColor::Red, false, DebugLifetime);
				DrawDebugSphere(GetWorld(), Hit.Location, SweepRadius, 12, FColor::Red, false, DebugLifetime);
				DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 6.f, 8, FColor::Red, false, DebugLifetime);
			}
		}
		if (bHit)
		{
			const FVector Contact = FMath::Lerp(Previous, Next, Hit.Time);
			if (Path) Path->Add(Contact);
			return Contact;
		}
		if (Path) Path->Add(Next);
		Previous = Next;
	}
	return Previous;
}

FVector UPhysicsManipulationComponent::GetThrowReleaseAdjustment(const UPrimitiveComponent& Component, const FVector& Start) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh || !Mesh->GetPhysicsAsset() || ThrowMaxReleaseAdjustment <= 0.0f) return FVector::ZeroVector;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ThrowReleaseClearance), false, GetOwner());
	Query.AddIgnoredActor(Component.GetOwner());
	AddThrowIgnoredCharacters(*GetWorld(), Query);
	const float Radius = Component.Bounds.SphereRadius;
	FVector Center = Start;
	double Remaining = ThrowMaxReleaseAdjustment;
	for (int32 Pass = 0; Pass < 3 && Remaining > UE_KINDA_SMALL_NUMBER; ++Pass)
	{
		FClosestPointOnPhysicsAsset Surface;
		if (!Mesh->GetClosestPointOnPhysicsAsset(Center, Surface, false) || Surface.Distance < 0.0f || Surface.Distance >= Radius + ThrowReleaseClearance) break;
		const FBodyInstance* Body = Mesh->GetBodyInstance(Surface.BoneName);
		if (!Body) break;
		const FVector Fallback = (Center - Character->GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER, Character->GetActorForwardVector());
		const FVector Outward = (Center - Body->GetCOMPosition()).GetSafeNormal(UE_SMALL_NUMBER, Fallback);
		const FBox Bounds = Body->GetBodyBounds();
		if (!Bounds.IsValid) break;
		const double Distance = FMath::Clamp(
		    FVector::DotProduct(Bounds.GetExtent(), Outward.GetAbs()) + Radius + ThrowReleaseClearance - FVector::DotProduct(Center - Bounds.GetCenter(), Outward), 0.0, Remaining);
		if (Distance <= UE_KINDA_SMALL_NUMBER) break;
		FHitResult Hit;
		double Fraction = 1.0;
		if (GetWorld()->SweepSingleByChannel(Hit, Center, Center + Outward * Distance, FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeSphere(Radius), Query))
		{
			Fraction = Hit.bStartPenetrating ? 0.0 : FMath::Max(static_cast<double>(Hit.Time) - ThrowReleaseClearance / Distance, 0.0);
		}
		if (Fraction <= UE_KINDA_SMALL_NUMBER) break;
		const FVector Delta = Outward * (Distance * Fraction);
		Center += Delta;
		Remaining -= Delta.Size();
	}
	return Center - Start;
}

void UPhysicsManipulationComponent::ReleaseComponent()
{
	ClearPickupPlayback();
	if (IsDragging())
	{
		if (UPrimitiveComponent* Component = ManipulatedComponent.Get();
		    Component && Component->GetAngularDamping() == AppliedDragAngularDamping && AppliedDragAngularDamping != SavedDragAngularDamping)
		{
			Component->SetAngularDamping(SavedDragAngularDamping);
		}
	}
	if (ACharacter* Character = DragCharacter.Get())
	{
		if (AStageShiftCharacter* Doll = Cast<AStageShiftCharacter>(Character))
		{
			Doll->GetDollPhysics()->SetDragging(false);
		}
		Character->GetCharacterMovement()->bOrientRotationToMovement = bSavedOrientToMovement;
		Character->GetCharacterMovement()->bUseControllerDesiredRotation = bSavedControllerDesiredRotation;
		Character->bUseControllerRotationYaw = bSavedControllerYaw;
	}
	DragCharacter.Reset();
	DragBoneName = NAME_None;
	DragEffort = 0.0f;
	DragTargetYaw = 0.0f;
	if (AActor* Actor = ManipulatedActor.Get())
	{
		Actor->OnEndPlay.RemoveDynamic(this, &ThisClass::OnManipulatedActorEndPlay);
	}

	if (UPrimitiveComponent* Component = ManipulatedComponent.Get())
	{
		if (bCarryAttached)
		{
			Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			Component->SetCollisionEnabled(PreviousCollision);
			Component->SetSimulatePhysics(bWasSimulatingPhysics);
			if (auto* Prop = Cast<AStagePhysicsActor>(ManipulatedActor.Get())) Prop->RefreshPickupOutline();
		}
	}
	ManipulatedComponent.Reset();
	ManipulatedActor.Reset();
	ActiveType = EPhysicsInteractionType::None;
	bPickupPending = false;
	bCarryAttached = false;
	ThrowAim = {};
	SetComponentTickEnabled(false);
	Super::ReleaseComponent();
	UpdateStagePropReadiness();
}

void UPhysicsManipulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (IsDragging())
	{
		UpdateDrag(DeltaTime);
		return;
	}
	if (bPickupPending)
	{
		UAnimInstance* Instance = PickupInstance.Get();
		if (!ManipulatedComponent.IsValid() || !ManipulatedComponent->IsSimulatingPhysics() || !Instance || !Instance->Montage_IsActive(PickupMontage.Get()))
		{
			if (UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
			ReleaseComponent();
		}
		return;
	}
	// A disabled/destroyed body cannot leave a stale active interaction behind.
	CancelManipulation();
}

void UPhysicsManipulationComponent::OnUnregister()
{
	CancelManipulation();
	Super::OnUnregister();
}

void UPhysicsManipulationComponent::OnManipulatedActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	ReleaseComponent();
}

void UPhysicsManipulationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bPropPlayEnded = true;
	if (AActor* Owner = GetOwner())
		if (auto* Physics = Owner->FindComponentByClass<UPhysicalMontageComponent>())
		{
			Physics->OnPhysAnimStarted.RemoveDynamic(this, &ThisClass::UpdateStagePropReadiness);
			Physics->OnRecoveryEnded.RemoveDynamic(this, &ThisClass::UpdateStagePropReadiness);
		}
	ReleaseComponent();
	Super::EndPlay(EndPlayReason);
}
