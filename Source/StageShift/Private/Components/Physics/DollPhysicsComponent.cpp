#include "Components/Physics/DollPhysicsComponent.h"

#include "Animation/StageShiftAnimInstance.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/StageShiftCharacter.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "TwoBoneIK.h"

namespace
{
	const FName DollSet(TEXT("Doll"));
	const FName DollPelvisBone(TEXT("pelvis"));
	constexpr int32 MaxContacts = 4;
}

UDollPhysicsComponent::UDollPhysicsComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bTickEvenWhenPaused = false;
	PelvisDrive = {9.0f, 6.0f, 0.8f, 9000.0f, 900.0f, 1.0f, 1.0f};
	BodyDrive = {3.5f, 2.0f, 0.7f, 6500.0f, 450.0f, 0.8f, 0.28f};
	HeadDrive = {3.5f, 3.0f, 0.85f, 6000.0f, 600.0f, 0.85f, 0.5f};
	ArmDrive = {3.0f, 2.0f, 0.8f, 6500.0f, 1800.0f, 0.75f, 0.6f};
	LegDrive = {5.5f, 4.0f, 0.8f, 60000.0f, 9000.0f, 0.8f, 0.75f};
}

void UDollPhysicsComponent::BeginPlay()
{
	Super::BeginPlay();
	Character = Cast<AStageShiftCharacter>(GetOwner());
	Mesh = Character.IsValid() ? Character->GetMesh() : nullptr;
	if (Mesh.IsValid())
	{
		AddTickPrerequisiteComponent(Mesh.Get());
		AddTickPrerequisiteComponent(Character->GetCharacterMovement());
		Mesh->OnComponentHit.AddUniqueDynamic(this, &ThisClass::OnBodyHit);
	}
	Contacts.Reserve(MaxContacts);
}

void UDollPhysicsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDriving(true);
	if (Mesh.IsValid())
	{
		Mesh->OnComponentHit.RemoveDynamic(this, &ThisClass::OnBodyHit);
	}
	Super::EndPlay(EndPlayReason);
}

const FStageShiftDollDrive& UDollPhysicsComponent::GetDrive(FName Bone) const
{
	if (Bone == DollPelvisBone) return PelvisDrive;
	if (Mesh->BoneIsChildOf(Bone, TEXT("head")) || Bone == TEXT("head")) return HeadDrive;
	for (const FName Root : {FName(TEXT("upperarm_l")), FName(TEXT("upperarm_r"))})
	{
		if (Bone == Root || Mesh->BoneIsChildOf(Bone, Root)) return ArmDrive;
	}
	for (const FName Root : {FName(TEXT("thigh_l")), FName(TEXT("thigh_r"))})
	{
		if (Bone == Root || Mesh->BoneIsChildOf(Bone, Root)) return LegDrive;
	}
	return BodyDrive;
}

bool UDollPhysicsComponent::StartDriving()
{
	USkeletalMeshComponent* BodyMesh = Mesh.Get();
	const UPhysicsAsset* Asset = BodyMesh ? BodyMesh->GetPhysicsAsset() : nullptr;
	if (!Asset || !BodyMesh->GetAnimInstance() || !BodyMesh->GetBodyInstance(DollPelvisBone)) return false;
	if (!BodyMesh->GetBodyInstance(DollPelvisBone)->IsValidBodyInstance()) return false;

	BodyMesh->HandleExistingParallelEvaluationTask(true, true);
	if (BodyMesh->GetEditableComponentSpaceTransforms().IsEmpty()) return false;
	if (UPhysicalMontageComponent* Montage = Character->FindComponentByClass<UPhysicalMontageComponent>())
	{
		Montage->SetStrengthMultiplyer(0.0f);
	}
	SavedCollision = BodyMesh->GetCollisionEnabled();
	SavedObjectType = BodyMesh->GetCollisionObjectType();
	SavedResponses = BodyMesh->GetCollisionResponseToChannels();
	SavedAnimTickOption = BodyMesh->VisibilityBasedAnimTickOption;
	bMeshSettingsSaved = true;
	BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	BodyMesh->SetCollisionObjectType(ECC_PhysicsBody);
	BodyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BodyMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	BodyMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	BodyMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	BodyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	for (const USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
	{
		FBodyInstance* Body = Setup ? BodyMesh->GetBodyInstance(Setup->BoneName) : nullptr;
		if (!Body || !Body->IsValidBodyInstance()) continue;

		const FStageShiftDollDrive& Settings = GetDrive(Setup->BoneName);
		FPhysicsControlData Data;
		Data.bUseSkeletalAnimation = false;

		Data.bUseAccelerationDriveMode = true;
		Data.LinearStrength = Settings.PositionFrequency;
		Data.LinearDampingRatio = Settings.DampingRatio;
		Data.MaxForce = FMath::Max(Settings.MaxAcceleration, 100.0f);
		Data.AngularStrength = Settings.RotationFrequency;
		Data.AngularDampingRatio = Settings.DampingRatio;
		Data.MaxTorque = FMath::Max(Settings.MaxAngularAcceleration, 1.0f);

		FPhysicsControlTarget Target;
		Target.bApplyControlPointToTarget = true;
		const FName Control = CreateControl(nullptr, NAME_None, BodyMesh, Setup->BoneName, Data, Target, DollSet);
		if (Control.IsNone()) continue;

		FDrivenBody& Driven = DrivenBodies.AddDefaulted_GetRef();
		Driven.Bone = Setup->BoneName;
		Driven.Control = Control;
		if (&Settings == &LegDrive) Driven.JointIndex = Asset->FindConstraintIndex(Driven.Bone);
		Driven.bWasSimulating = Body->IsInstanceSimulatingPhysics();
		Driven.bWasCCD = Body->bUseCCD;
		Driven.bWasNotifying = Body->bNotifyRigidBodyCollision;
		Driven.BlendWeight = Body->PhysicsBlendWeight;
		TotalDrivenMass += FMath::Max(Body->GetBodyMass(), 0.0f);
		Body->SetInstanceNotifyRBCollision(true);
	}
	if (DrivenBodies.IsEmpty())
	{
		StopDriving(true);
		return false;
	}
	
	FPhysicsControlModifierData Modifier;
	Modifier.MovementType = EPhysicsMovementType::Simulated;
	Modifier.CollisionType = ECollisionEnabled::QueryAndPhysics;
	Modifier.PhysicsBlendWeight = 1.0f;
	Modifier.bEnableCCD = true;
	for (const FDrivenBody& Driven : DrivenBodies)
	{
		CreateBodyModifier(BodyMesh, Driven.Bone, DollSet, Modifier);
	}
	bDriving = true;
	SetDragging(bDragging);
	bResetPending = true;
	PreviousComponentTransform = BodyMesh->GetComponentTransform();
	return true;
}

void UDollPhysicsComponent::StopDriving(bool bRestoreAnimation)
{
	if (!DrivenBodies.IsEmpty()) DestroyAllControlsAndBodyModifiers();
	DragSelfCollisionControls.Reset();
	if (USkeletalMeshComponent* BodyMesh = Mesh.Get(); BodyMesh && bMeshSettingsSaved)
	{
		for (const FDrivenBody& Driven : DrivenBodies)
		{
			const UPhysicsAsset* Asset = BodyMesh->GetPhysicsAsset();
			if (Driven.bLimitsWidened && Asset && Asset->ConstraintSetup.IsValidIndex(Driven.JointIndex))
			{
				if (FConstraintInstance* Joint = BodyMesh->GetConstraintInstanceByIndex(Driven.JointIndex))
				{
					Joint->RestoreAngularLimitsToDefault(Asset->ConstraintSetup[Driven.JointIndex]->DefaultInstance);
				}
			}
			if (FBodyInstance* Body = BodyMesh->GetBodyInstance(Driven.Bone))
			{
				Body->SetUseCCD(Driven.bWasCCD);
				Body->SetInstanceNotifyRBCollision(Driven.bWasNotifying);
				if (bRestoreAnimation)
				{
					Body->SetInstanceSimulatePhysics(Driven.bWasSimulating);
					Body->PhysicsBlendWeight = Driven.BlendWeight;
				}
			}
		}
		BodyMesh->VisibilityBasedAnimTickOption = SavedAnimTickOption;
		if (bRestoreAnimation)
		{
			BodyMesh->SetCollisionEnabled(SavedCollision);
			BodyMesh->SetCollisionObjectType(SavedObjectType);
			BodyMesh->SetCollisionResponseToChannels(SavedResponses);
		}
	}
	DrivenBodies.Reset();
	TotalDrivenMass = 0.0f;
	Contacts.Reset();
	InteractionNormal = FVector::ZeroVector;
	DynamicContactComponent.Reset();
	DynamicContactMass = 0.0f;
	LastDynamicContactTime = -1.0e10;
	bDriving = false;
	bMeshSettingsSaved = false;
}

void UDollPhysicsComponent::SuspendForRagdoll()
{
	StopDriving(false);
	bSuspended = true;
}

void UDollPhysicsComponent::SetDragging(bool bIsDragging)
{
	for (FName Control : DragSelfCollisionControls) DestroyControl(Control, true, false);
	DragSelfCollisionControls.Reset();
	bDragging = bIsDragging;
	USkeletalMeshComponent* BodyMesh = Mesh.Get();
	if (!bDriving || !BodyMesh || !bDragging) return;

	// Only filter opposing leg pairs during the low reach pose. The dragged prop
	// keeps colliding with every character body, including both feet and hands.
	FPhysicsControlData Data;
	Data.bUseSkeletalAnimation = false;
	Data.bDisableCollision = true;
	const FName Set(TEXT("DragSelfCollisionExclusions"));
	const auto Exclude = [&](FName ParentBone, FName ChildBone)
	{
		const FBodyInstance* Parent = BodyMesh->GetBodyInstance(ParentBone);
		const FBodyInstance* Child = BodyMesh->GetBodyInstance(ChildBone);
		if (!Parent || !Child || !Parent->IsValidBodyInstance() || !Child->IsValidBodyInstance()) return;
		const FName Control = CreateControl(BodyMesh, ParentBone, BodyMesh, ChildBone, Data, FPhysicsControlTarget(), Set);
		if (!Control.IsNone()) DragSelfCollisionControls.Add(Control);
	};
	const FName Left[] = {TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l")};
	const FName Right[] = {TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")};
	// A low two-hand reach must not make the two legs jam against each other.
	const UPhysicsAsset* Asset = BodyMesh->GetPhysicsAsset();
	if (!Asset) return;
	for (FName LeftBone : Left)
	{
		for (FName RightBone : Right)
		{
			const int32 LeftIndex = Asset->FindBodyIndex(LeftBone);
			const int32 RightIndex = Asset->FindBodyIndex(RightBone);
			if (LeftIndex != INDEX_NONE && RightIndex != INDEX_NONE && Asset->IsCollisionEnabled(LeftIndex, RightIndex))
			{
				Exclude(LeftBone, RightBone);
			}
		}
	}
}

void UDollPhysicsComponent::ResetAfterTeleport()
{
	bResetPending = true;
}

void UDollPhysicsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	USceneComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (TickType != LEVELTICK_All || DeltaTime <= UE_SMALL_NUMBER || !Character.IsValid() || !Mesh.IsValid()) return;
	if (!bEnabled)
	{
		if (bDriving) StopDriving(true);
		return;
	}
	if (bSuspended)
	{
		if (Character->IsPhysicsMontageActive()) return;
		bSuspended = false;
	}
	if (!bDriving && (Character->IsPhysicsMontageActive() || !StartDriving())) return;

	const FTransform ComponentTransform = Mesh->GetComponentTransform();
	if (FVector::DistSquared(ComponentTransform.GetLocation(), PreviousComponentTransform.GetLocation()) > FMath::Square(100.0) ||
		ComponentTransform.GetRotation().AngularDistance(PreviousComponentTransform.GetRotation()) > PI * 0.6)
	{
		bResetPending = true;
	}
	PreviousComponentTransform = ComponentTransform;
	
	if (bResetPending) SetCachedBoneVelocitiesToZero();
	UpdateTargetCaches(DeltaTime);
	UpdateLegTargets();
	UpdateLegJointLimits();
	if (bResetPending)
	{
		ResetBodyModifiersInSetToCachedBoneTransforms(DollSet);
		Contacts.Reset();
		InteractionNormal = FVector::ZeroVector;
		DynamicContactComponent.Reset();
		DynamicContactMass = 0.0f;
		LastDynamicContactTime = -1.0e10;
	}
	UpdateDriveTargets(DeltaTime);
	UpdateContacts(DeltaTime);
	UpdateControls(DeltaTime);
	bResetPending = false;
}

void UDollPhysicsComponent::UpdateLegTargets()
{
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const UPhysicsAsset* Asset = Mesh->GetPhysicsAsset();
	if (!Movement->IsMovingOnGround() || !Asset) return;

	FCollisionQueryParams Query(SCENE_QUERY_STAT(DollFootClearance), false, Character.Get());
	Query.AddIgnoredComponent(Character->GetHeldPhysicsComponent());
	const FCollisionObjectQueryParams Objects(ECC_TO_BITFIELD(ECC_WorldStatic) | ECC_TO_BITFIELD(ECC_WorldDynamic));
	const double MaxSupportHeight = Movement->CurrentFloor.HitResult.ImpactPoint.Z + Movement->MaxStepHeight;
	TArray<FHitResult> Hits;
	const FName Thighs[] = {TEXT("thigh_l"), TEXT("thigh_r")};
	const FName Calves[] = {TEXT("calf_l"), TEXT("calf_r")};
	const FName Feet[] = {TEXT("foot_l"), TEXT("foot_r")};
	for (int32 Side = 0; Side < UE_ARRAY_COUNT(Feet); ++Side)
	{
		const int32 BodyIndex = Asset->FindBodyIndex(Feet[Side]);
		if (BodyIndex == INDEX_NONE) continue;
		const USkeletalBodySetup* Setup = Asset->SkeletalBodySetups[BodyIndex];
		FTransform Foot = GetCachedBoneTransform(Mesh.Get(), Feet[Side]);
		const FVector Scale = Foot.GetScale3D().GetAbs();
		double Lift = 0.0;
		const auto CheckShape = [&](const FVector& Center, const FQuat& Rotation, const FCollisionShape& Shape)
		{
			const double SearchHeight = Movement->MaxStepHeight + Shape.GetExtent().Size() * 2.0;
			Hits.Reset();
			GetWorld()->SweepMultiByObjectType(Hits, Center + FVector::UpVector * SearchHeight, Center,
				Rotation, Objects, Shape, Query);
			for (const FHitResult& Hit : Hits)
			{
				const UPrimitiveComponent* Support = Hit.GetComponent();
				if (!Support || Hit.bStartPenetrating || Support->IsSimulatingPhysics(Hit.BoneName) ||
					Support->GetCollisionResponseToChannel(ECC_PhysicsBody) != ECR_Block ||
					Hit.ImpactNormal.Z < Movement->GetWalkableFloorZ() || Hit.ImpactPoint.Z > MaxSupportHeight + 0.5) continue;
				Lift = FMath::Max(Lift, Hit.Location.Z - Center.Z + 0.1);
			}
		};
		for (const FKBoxElem& Box : Setup->AggGeom.BoxElems)
		{
			CheckShape(Foot.TransformPosition(Box.Center), Foot.TransformRotation(Box.Rotation.Quaternion()),
				FCollisionShape::MakeBox(FVector(Box.X, Box.Y, Box.Z) * Scale * 0.5));
		}
		for (const FKSphylElem& Capsule : Setup->AggGeom.SphylElems)
		{
			const double Radius = Capsule.Radius * FMath::Min(Scale.X, Scale.Y);
			CheckShape(Foot.TransformPosition(Capsule.Center), Foot.TransformRotation(Capsule.Rotation.Quaternion()),
				FCollisionShape::MakeCapsule(Radius, Capsule.Length * Scale.Z * 0.5 + Radius));
		}
		if (Lift < 0.1) continue;
		
		FTransform Thigh = GetCachedBoneTransform(Mesh.Get(), Thighs[Side]);
		FTransform Calf = GetCachedBoneTransform(Mesh.Get(), Calves[Side]);
		const FVector Goal = Foot.GetLocation() + FVector::UpVector * Lift;
		const FQuat FootRotation = Foot.GetRotation();
		const FVector LegAxis = (Foot.GetLocation() - Thigh.GetLocation()).GetSafeNormal();
		const FVector Bend = FVector::VectorPlaneProject(Calf.GetLocation() - Thigh.GetLocation(), LegAxis);
		const FVector Pole = Calf.GetLocation() + (Bend.IsNearlyZero(0.05) ? Character->GetActorForwardVector() * 20.0 : FVector::ZeroVector);
		AnimationCore::SolveTwoBoneIK(Thigh, Calf, Foot, Pole, Goal, false, 1.0, 1.0);
		Foot.SetRotation(FootRotation);
		SetCachedBoneData(Mesh.Get(), Thighs[Side], Thigh);
		SetCachedBoneData(Mesh.Get(), Calves[Side], Calf);
		SetCachedBoneData(Mesh.Get(), Feet[Side], Foot);
	}
}

void UDollPhysicsComponent::UpdateLegJointLimits()
{
	const UPhysicsAsset* Asset = Mesh->GetPhysicsAsset();
	if (!Asset) return;
	for (FDrivenBody& Driven : DrivenBodies)
	{
		if (!Asset->ConstraintSetup.IsValidIndex(Driven.JointIndex)) continue;
		FConstraintInstance* Joint = Mesh->GetConstraintInstanceByIndex(Driven.JointIndex);
		if (!Joint) continue;
		const FConstraintInstance& Authored = Asset->ConstraintSetup[Driven.JointIndex]->DefaultInstance;
		const FQuat Child = GetCachedBoneTransform(Mesh.Get(), Authored.ConstraintBone1).GetRotation();
		const FQuat Parent = GetCachedBoneTransform(Mesh.Get(), Authored.ConstraintBone2).GetRotation();
		// Keep an envelope of the evaluated gait poses. Shrinking it onto a moving leg injects
		// constraint impulses; restore the authored envelope when relinquishing the drives.
		Driven.bLimitsWidened |= Joint->WidenLimitsForDriveTarget(Parent.Inverse() * Child, *Joint, 5.0f);
	}
}

void UDollPhysicsComponent::UpdateDriveTargets(float DeltaTime)
{
	const float InteractionWeight = 1.0f - FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - LastDynamicContactTime - 0.08) / 0.15), 0.0f, 1.0f);
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bPassivePushing = HasPassivePushContact() &&
		(FVector::DotProduct(Movement->GetCurrentAcceleration(), InteractionNormal) < -UE_KINDA_SMALL_NUMBER ||
		 FVector::DotProduct(Character->GetVelocity(), InteractionNormal) < -1.0);
	// All drives use acceleration mode. Divide one shared force budget across their
	// total mass, including the diagonal worst case for the two horizontal axes.
	const float PassiveAcceleration = FMath::Max(MaxPassivePushForce, 1.0f) * 100.0f /
		(FMath::Max(TotalDrivenMass, 0.1f) * FMath::Sqrt(2.0f));
	const float PassiveTorqueScale = FMath::Min(1.0f, PassiveAcceleration / FMath::Max(MaxInteractionAcceleration, 100.0f));
	for (FDrivenBody& Driven : DrivenBodies)
	{
		const FBodyInstance* Body = Mesh->GetBodyInstance(Driven.Bone);
		if (!Body) continue;
		const FStageShiftDollDrive& Settings = GetDrive(Driven.Bone);
		const bool bLimb = &Settings == &ArmDrive || &Settings == &LegDrive;
		const UStageShiftAnimInstance* Animation = Cast<UStageShiftAnimInstance>(Mesh->GetAnimInstance());
		const float Reach = &Settings == &ArmDrive && Animation ? Animation->PickupAlpha : 0.0f;
		FPhysicsControlMultiplier Multiplier;
		Multiplier.LinearStrengthMultiplier = FVector(FMath::Lerp(1.0f, 1.7f, Reach));
		Multiplier.AngularStrengthMultiplier = FMath::Lerp(1.0f, 1.4f, Reach);
		if (InteractionWeight > 0.0f)
		{
			// Yield into movable objects while retaining vertical support and tangent motion.
			const float ForceScale = FMath::Min(1.0f, MaxInteractionAcceleration / FMath::Max(Settings.MaxAcceleration, 100.0f));
			Multiplier.MaxForceMultiplier = FVector::OneVector - InteractionNormal.GetAbs() * ((1.0f - ForceScale) * InteractionWeight);
			const float TorqueScale = FMath::Min(1.0f, MaxInteractionAngularAcceleration / FMath::Max(Settings.MaxAngularAcceleration, 1.0f));
			Multiplier.MaxTorqueMultiplier = FMath::Lerp(1.0f, TorqueScale, InteractionWeight);
		}
		if (bPassivePushing)
		{
			// Cap the whole body's push, rather than granting the full budget to each
			// foot/limb. Vertical support and collisions remain fully active.
			const float ForceScale = FMath::Min(1.0f, PassiveAcceleration / FMath::Max(Settings.MaxAcceleration, 100.0f));
			Multiplier.MaxForceMultiplier.X = FMath::Min(Multiplier.MaxForceMultiplier.X, static_cast<double>(ForceScale));
			Multiplier.MaxForceMultiplier.Y = FMath::Min(Multiplier.MaxForceMultiplier.Y, static_cast<double>(ForceScale));
			const float TorqueScale = FMath::Min(1.0f, MaxInteractionAngularAcceleration * PassiveTorqueScale / FMath::Max(Settings.MaxAngularAcceleration, 1.0f));
			Multiplier.MaxTorqueMultiplier = FMath::Min(Multiplier.MaxTorqueMultiplier, TorqueScale);
		}
		SetControlMultiplier(Driven.Control, Multiplier);
		const FTransform Desired = GetCachedBoneTransform(Mesh.Get(), Driven.Bone);
		const FTransform Actual = Body->GetUnrealWorldTransform();
		Driven.AnimationTarget = Desired;
		FPhysicsControlTarget Target;
		Target.bApplyControlPointToTarget = true;
		Target.TargetPosition = Actual.GetLocation() + (Desired.GetLocation() - Actual.GetLocation()).GetClampedToMaxSize(MaxTargetError);
		const FQuat Difference = Desired.GetRotation() * Actual.GetRotation().Inverse();
		const double Angle = Difference.AngularDistance(FQuat::Identity);
		const double RotationAlpha = FMath::Min(1.0, FMath::DegreesToRadians(65.0) / FMath::Max(Angle, UE_SMALL_NUMBER));
		Target.TargetOrientation = FQuat::Slerp(Actual.GetRotation(), Desired.GetRotation(), RotationAlpha).Rotator();
		if (!bResetPending)
		{
			const float FeedForward = FMath::Lerp(Settings.VelocityFeedForward, 1.0f, Reach);
			const float SpeedLimit = bLimb ? MaxLimbTargetSpeed : MaxTargetSpeed;
			Target.TargetVelocity = ((Desired.GetLocation() - Driven.PreviousTarget.GetLocation()) / DeltaTime).GetClampedToMaxSize(SpeedLimit) * FeedForward;
			const FQuat RotationDelta = (Desired.GetRotation() * Driven.PreviousTarget.GetRotation().Inverse()).GetShortestArcWith(FQuat::Identity);
			
			const float AngularFeedForward = FMath::Lerp(Settings.AngularVelocityFeedForward, 1.0f, Reach);
			Target.TargetAngularVelocity = (RotationDelta.ToRotationVector() / (2.0 * PI * DeltaTime)).GetClampedToMaxSize(bLimb ? 4.0 : 2.0) * AngularFeedForward;
		}
		SetControlTarget(Driven.Control, Target);
		Driven.PreviousTarget = Desired;
	}
}

bool UDollPhysicsComponent::HasPassivePushContact() const
{
	return !bDragging && DynamicContactComponent.IsValid() && DynamicContactComponent->IsRegistered() &&
		DynamicContactComponent.Get() != Character->GetHeldPhysicsComponent() &&
		GetWorld()->GetTimeSeconds() - LastDynamicContactTime < 0.12 && !InteractionNormal.IsNearlyZero();
}

FVector UDollPhysicsComponent::ConstrainMovement(const FVector& Delta) const
{
	if (!bDriving || bResetPending || !Mesh.IsValid()) return Delta;
	FVector LimitedDelta = Delta;
	if (HasPassivePushContact())
	{
		const FVector Into = -InteractionNormal;
		const double SpeedInto = FVector::DotProduct(Character->GetVelocity(), Into);
		const double MoveInto = FVector::DotProduct(LimitedDelta, Into);
		const float PushSpeed = FMath::Max(MaxPassivePushSpeed, 1.0f) / FMath::Sqrt(1.0f + FMath::Max(DynamicContactMass, 0.0f) / 25.0f);
		if (MoveInto > 0.0 && SpeedInto > PushSpeed)
		{
			// Sprint cannot bypass this cap. Backing away and sliding along the prop
			// keep their full movement; active Drag uses its own mass/reach limits.
			LimitedDelta -= Into * (MoveInto * (1.0 - PushSpeed / SpeedInto));
		}
	}
	const FDrivenBody* Root = DrivenBodies.FindByPredicate([](const FDrivenBody& Body) { return Body.Bone == DollPelvisBone; });
	const FBodyInstance* Body = Mesh->GetBodyInstance(DollPelvisBone);
	if (!Root || !Body) return LimitedDelta;
	const FVector Error = (Root->AnimationTarget.GetLocation() - Body->GetUnrealWorldTransform().GetLocation()) * FVector(1.0, 1.0, 0.0);
	const FVector Proposed = Error + FVector(LimitedDelta.X, LimitedDelta.Y, 0.0);
	if (Proposed.SizeSquared() <= FMath::Square(RootLeash)) return LimitedDelta;
	const FVector Away = Proposed.GetSafeNormal();
	const double SeparatingMove = FVector::DotProduct(LimitedDelta, Away);
	return LimitedDelta - Away * FMath::Clamp(Proposed.Size() - RootLeash, 0.0, FMath::Max(SeparatingMove, 0.0));
}

void UDollPhysicsComponent::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bDriving || !OtherComponent || OtherActor == GetOwner()) return;
	Character->ConsiderPhysicsHit(Hit, NormalImpulse);
	if (OtherComponent->IsSimulatingPhysics(Hit.BoneName))
	{
		const FVector Normal = Hit.ImpactNormal.GetSafeNormal2D();
		if (!Normal.IsNearlyZero())
		{
			InteractionNormal = Normal;
			DynamicContactComponent = OtherComponent;
			const FBodyInstance* OtherBody = OtherComponent->GetBodyInstance(Hit.BoneName);
			DynamicContactMass = FMath::Max(OtherBody ? OtherBody->GetBodyMass() : OtherComponent->GetMass(), 0.1f);
			LastDynamicContactTime = GetWorld()->GetTimeSeconds();
		}
	}
	Character->ConsiderStumble(Hit);
	const FName Bone = Hit.MyBoneName;
	if (Bone.IsNone()) return;
	if ((Bone == TEXT("foot_l") || Bone == TEXT("foot_r")) && Hit.ImpactNormal.Z > 0.8) return;
	const FBodyInstance* Body = Mesh->GetBodyInstance(Bone);
	if (!Body) return;
	const FVector LocalPoint = Body->GetUnrealWorldTransform().InverseTransformPosition(Hit.ImpactPoint);
	FStageShiftDollContact* Contact = Contacts.FindByPredicate([&](const FStageShiftDollContact& Item)
	{
		return Item.Bone == Bone && FVector::DotProduct(Item.Normal, Hit.ImpactNormal) > 0.5 && FVector::DistSquared(Item.LocalPoint, LocalPoint) < FMath::Square(15.0);
	});
	if (!Contact)
	{
		if (Contacts.Num() == MaxContacts)
		{
			Contact = &Contacts[0];
			for (FStageShiftDollContact& Item : Contacts)
			{
				if (Item.LastContactTime < Contact->LastContactTime) Contact = &Item;
			}
			*Contact = {};
		}
		else Contact = &Contacts.AddDefaulted_GetRef();
	}
	const double DeltaSpeed = NormalImpulse.Size() / FMath::Max(Body->GetBodyMass(), 0.01f);
	Contact->Bone = Bone;
	Contact->LocalPoint = LocalPoint;
	Contact->Normal = Hit.ImpactNormal.GetSafeNormal();
	
	Contact->ImpactDepth = FMath::Max(Contact->ImpactDepth, FMath::Clamp(DeltaSpeed * 0.015, 0.0, static_cast<double>(MaxContactDepth)));
	Contact->LastContactTime = GetWorld()->GetTimeSeconds();
}

void UDollPhysicsComponent::UpdateContacts(float DeltaTime)
{
	const double Now = GetWorld()->GetTimeSeconds();
	for (FStageShiftDollContact& Contact : Contacts)
	{
		float Pressure = 0.0f;
		if (Now - Contact.LastContactTime < 0.12)
		{
			const FDrivenBody* Driven = DrivenBodies.FindByPredicate([&Contact](const FDrivenBody& Body) { return Body.Bone == Contact.Bone; });
			const FBodyInstance* Body = Mesh->GetBodyInstance(Contact.Bone);
			if (Driven && Body)
			{
				Pressure = FMath::Clamp(-FVector::DotProduct(Driven->AnimationTarget.GetLocation() - Body->GetUnrealWorldTransform().GetLocation(), Contact.Normal) * 0.25, 0.0, static_cast<double>(MaxContactDepth));
			}
		}
		const float Target = FMath::Max(Pressure, Contact.ImpactDepth);
		FMath::SpringDamper(Contact.Depth, Contact.Velocity, Target, 0.0f, DeltaTime, 7.0f, 0.65f);
		Contact.Depth = FMath::Clamp(Contact.Depth, 0.0f, MaxContactDepth);
		Contact.ImpactDepth *= FMath::Exp(-18.0f * DeltaTime);
	}
	Contacts.RemoveAll([Now](const FStageShiftDollContact& Contact)
	{
		return Now - Contact.LastContactTime > 0.3 && Contact.Depth < 0.01f;
	});
}
