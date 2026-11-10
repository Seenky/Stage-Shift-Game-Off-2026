// Fill out your copyright notice in the Description page of Project Settings.

#include "StageShift/Public/Physics/PhysicsManipulationComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Components/Interaction/ThrowAimComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Core/StageShiftCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/Physics/DollPhysicsComponent.h"

UPhysicsManipulationComponent::UPhysicsManipulationComponent()
{
	bAutoActivate = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPhysicsManipulationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner())) AddTickPrerequisiteComponent(Character->GetCharacterMovement());
}

void UPhysicsManipulationComponent::SetCarryPoint(USceneComponent* NewCarryPoint)
{
	CarryPoint = NewCarryPoint;
}

bool UPhysicsManipulationComponent::CanManipulate(const FHitResult& Hit, EPhysicsInteractionType Type) const
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (!IsValid(GetOwner()) || !IsValid(Component) || ManipulatedComponent.IsValid() || GetGrabbedComponent()
		|| !Component->IsRegistered() || Component->GetWorld() != GetWorld() || Component->GetOwner() == GetOwner()
		|| !Component->IsSimulatingPhysics(Hit.BoneName))
	{
		return false;
	}

	if (Type == EPhysicsInteractionType::Carry)
	{
		return CarryPoint.IsValid() && CarryPoint->GetOwner() == GetOwner() && Component == Component->GetOwner()->GetRootComponent();
	}
	
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const AStageShiftCharacter* Doll = Cast<AStageShiftCharacter>(GetOwner());
	return Type == EPhysicsInteractionType::Drag && Character && Character->GetCharacterMovement()->IsMovingOnGround() &&
		(!Doll || !Doll->IsPhysicsMontageActive());
}

bool UPhysicsManipulationComponent::BeginManipulation(const FHitResult& Hit, EPhysicsInteractionType Type)
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
		const bool bHasGrabNotify = Montage && Montage->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
		{
			return Event.Notify && Event.Notify->GetNotifyName() == TEXT("GrabObject");
		});
		if (Instance && bHasGrabNotify)
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
		const FVector ReachStart = Mesh && Mesh->GetBoneIndex(Shoulder) != INDEX_NONE
			? Mesh->GetSocketLocation(Shoulder) : GetOwner()->GetActorLocation();
		ReachCenter += ReachStart * 0.5;
		FVector Point = GripPoint + Right * (DragHandSpacing * (Side == 0 ? -0.5f : 0.5f));
		if (Component->DoesSocketExist(Socket)) Point = Component->GetSocketLocation(Socket);
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
						Clearance = FMath::Max(Clearance,
							FVector::DotProduct(Bounds.GetExtent(), Normal.GetAbs()) - FVector::DotProduct(Offset, Normal) + 1.0);
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
	return FMath::Clamp(BaseScale / FMath::Sqrt(1.0f + Load),
		FMath::Clamp(DragMinimumSpeedMultiplier, 0.01f, BaseScale), BaseScale);
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
	Facing = ToGrip.SizeSquared2D() > FMath::Square(FacingRadius)
		? ToGrip.GetSafeNormal2D() : GetOwner()->GetActorForwardVector();
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
	if (!Character || !Component || !Component->IsRegistered() || !Component->IsSimulatingPhysics(DragBoneName) ||
		!Character->GetCharacterMovement()->IsMovingOnGround() || (Doll && Doll->IsPhysicsMontageActive()))
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
	const FVector ReachStart = Mesh && Mesh->GetBoneIndex(TEXT("upperarm_r")) != INDEX_NONE
		? Mesh->GetSocketLocation(TEXT("upperarm_r")) : OwnerLocation;
	const FVector GripNormal = GetDragFrame().TransformVectorNoScale(DragLocalGripNormal);
	const bool bGripTurnedAway = FVector::DotProduct(GripNormal, (ReachStart - Center).GetSafeNormal()) < 0.25;
	if (Distance + ReachSlack < DragRestDistance || bGripTurnedAway)
	{
		// Recover a reachable surface after the prop falls or rolls over the old grip.
		// Only the hands/reach change; the prop stays fully simulated and collidable.
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
		const float FacingLead = Distance > Character->GetCapsuleComponent()->GetScaledCapsuleRadius()
			? FMath::Clamp(Lead, -12.0f, 12.0f) : 0.0f;
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
	const FVector DriveVelocity = Input.IsNearlyZero() ? Character->GetVelocity()
		: (Input / FMath::Max(Movement->GetMaxAcceleration(), 1.0f)).GetClampedToMaxSize(1.0) * Movement->GetMaxSpeed();
	// Input keeps exerting effort when the player's reach leash blocks further movement.
	const FVector RelativeVelocity = (DriveVelocity + TurnVelocity - Component->GetPhysicsLinearVelocity(DragBoneName)) * FVector(1, 1, 0);
	// Tune all force-related settings in units of the same reference load.
	// 100 kg at a 100 kg reference has the response of 25 kg at the default 25 kg reference.
	constexpr float TuningReferenceMass = 25.0f;
	const float StrengthScale = FMath::Max(DragReferenceMass, 0.1f) / TuningReferenceMass;
	const float Stiffness = FMath::Max(DragSpringStiffness, 1.0f) * StrengthScale;
	const float Damping = 2.0f * FMath::Max(DragDampingRatio, 0.1f) * FMath::Sqrt(Stiffness * DragMass);
	const double ForceLimit = FMath::Min(FMath::Max(DragMaxForce, 1.0f) * StrengthScale * 100.0,
		static_cast<double>(DragMass * FMath::Max(DragMaxAcceleration, 1.0f)));
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
	bWasSimulatingPhysics = true;
	Component->SetSimulatePhysics(false);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	const FAttachmentTransformRules& Rules = bKeepWorldTransform
		? FAttachmentTransformRules::KeepWorldTransform
		: FAttachmentTransformRules::SnapToTargetNotIncludingScale;
	if (!Component->AttachToComponent(CarryPoint.Get(), Rules))
	{
		Component->SetCollisionEnabled(PreviousCollision);
		Component->SetSimulatePhysics(bWasSimulatingPhysics);
		return false;
	}
	bCarryAttached = true;
	return true;
}

bool UPhysicsManipulationComponent::GetPickupTarget(FVector& WorldLocation, float& Weight, FVector* HandOffset) const
{
	const UPrimitiveComponent* Component = ManipulatedComponent.Get();
	const UAnimInstance* Instance = PickupInstance.Get();
	if (!Component || !Instance || !Instance->Montage_IsActive(PickupMontage.Get())) return false;

	WorldLocation = bCarryAttached ? GrabLocation : Component->DoesSocketExist(GripSocket)
		? Component->GetSocketLocation(GripSocket)
		: Component->GetComponentTransform().TransformPosition(LocalGripLocation);
	Weight = FMath::Clamp(Instance->GetCurveValue(TEXT("PickupReach")), 0.0f, 1.0f);
	if (HandOffset && CarryPoint.IsValid())
	{
		const USkeletalMeshComponent* Mesh = Instance->GetSkelMeshComponent();
		const int32 HandIndex = Mesh ? Mesh->GetBoneIndex(TEXT("hand_r")) : INDEX_NONE;
		*HandOffset = HandIndex != INDEX_NONE
			? Mesh->GetBoneTransform(HandIndex).InverseTransformPosition(CarryPoint->GetComponentLocation()) : FVector::ZeroVector;
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
	// An unreachable or moving-away prop stays where it is instead of snapping to the hand.
	if (!CarryPoint.IsValid() || FVector::Dist(CarryPoint->GetComponentLocation(), GrabLocation) > 12.0f ||
		!AttachCarriedObject(true))
	{
		if (UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
		ReleaseComponent();
		return;
	}
	bPickupPending = false;
	SetComponentTickEnabled(false);
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
	const bool bThrow = bCarry && !bCancelled && HoldDuration > TapDuration;
	const FVector Forward = !ThrowDirection.IsNearlyZero() ? ThrowDirection : GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
	const bool bTargeted = bHasThrowTarget;
	const FVector Target = ThrowTarget;
	const FVector TargetNormal = ThrowTargetNormal;

	ReleaseComponent();
	
	if (IsValid(Component) && bCarry && Component->IsSimulatingPhysics() && IsValid(GetOwner()))
	{
		Component->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Component->SetPhysicsLinearVelocity(FVector::ZeroVector);
		if (bThrow)
		{
			const float Charge = FMath::Clamp((HoldDuration - TapDuration) / FMath::Max(MaxChargeDuration, UE_SMALL_NUMBER), 0.0f, 1.0f);
			const float Speed = FMath::Lerp(MinThrowSpeed, FMath::Max(MinThrowSpeed, MaxThrowSpeed), Charge);
			FVector Velocity;
			if (bTargeted)
			{
				const FVector CenterOffset = Component->Bounds.Origin - Component->GetCenterOfMass();
				const double SurfaceOffset = FVector::DotProduct(Component->Bounds.BoxExtent, TargetNormal.GetAbs())
					- FVector::DotProduct(CenterOffset, TargetNormal);
				Velocity = CalculateThrowVelocity(*Component, Target + TargetNormal * FMath::Max(SurfaceOffset, 0.0), Speed);
			}
			else
			{
				const FVector Direction = (Forward + FVector::UpVector * ThrowUpwardBias).GetSafeNormal();
				Velocity = GetOwner()->GetVelocity() + Direction * Speed;
			}
			Component->SetPhysicsLinearVelocity(Velocity);
		}
	}
}

void UPhysicsManipulationComponent::SetThrowTarget(const FVector& WorldLocation, const FVector& SurfaceNormal)
{
	bHasThrowTarget = !WorldLocation.ContainsNaN();
	ThrowTarget = WorldLocation;
	ThrowTargetNormal = SurfaceNormal.ContainsNaN() ? FVector::UpVector : SurfaceNormal.GetSafeNormal();
}

FVector UPhysicsManipulationComponent::CalculateThrowVelocity(const UPrimitiveComponent& Component, const FVector& Target, float Speed) const
{
	if (Speed <= UE_SMALL_NUMBER) return FVector::ZeroVector;
	const FVector Start = Component.GetCenterOfMass();
	const FVector Offset = Target - Start;
	const float GravityZ = Component.IsGravityEnabled() && GetWorld() ? GetWorld()->GetGravityZ() : 0.0f;
	if (FMath::Abs(GravityZ) > UE_SMALL_NUMBER && Offset.SizeSquared2D() > 1.0)
	{
		UGameplayStatics::FSuggestProjectileVelocityParameters Parameters(this, Start, Target, Speed);
		Parameters.OverrideGravityZ = GravityZ;
		Parameters.TraceOption = ESuggestProjVelocityTraceOption::DoNotTrace;
		Parameters.bAcceptClosestOnNoSolutions = true;
		FVector Velocity;
		if (UGameplayStatics::SuggestProjectileVelocity(Parameters, Velocity) && !Velocity.ContainsNaN()) return Velocity;
	}
	return Offset.IsNearlyZero() ? GetOwner()->GetActorForwardVector() * Speed : Offset.GetSafeNormal() * Speed;
}

void UPhysicsManipulationComponent::ReleaseComponent()
{
	ClearPickupPlayback();
	if (IsDragging())
	{
		if (UPrimitiveComponent* Component = ManipulatedComponent.Get(); Component &&
			Component->GetAngularDamping() == AppliedDragAngularDamping && AppliedDragAngularDamping != SavedDragAngularDamping)
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
		}
	}
	ManipulatedComponent.Reset();
	ManipulatedActor.Reset();
	ActiveType = EPhysicsInteractionType::None;
	bPickupPending = false;
	bCarryAttached = false;
	ThrowDirection = FVector::ZeroVector;
	bHasThrowTarget = false;
	SetComponentTickEnabled(false);
	Super::ReleaseComponent();
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
		if (!ManipulatedComponent.IsValid() || !ManipulatedComponent->IsSimulatingPhysics() ||
			!Instance || !Instance->Montage_IsActive(PickupMontage.Get()))
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
	ReleaseComponent();
	Super::EndPlay(EndPlayReason);
}
