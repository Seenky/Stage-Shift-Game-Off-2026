#include "Animation/StageShiftFootPlacement.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StageShiftFootPlacement)

namespace
{
	const FRigElementKey RootBone(TEXT("root"), ERigElementType::Bone);
	const FRigElementKey LeftFoot(TEXT("foot_l"), ERigElementType::Bone);
	const FRigElementKey RightFoot(TEXT("foot_r"), ERigElementType::Bone);
	const FRigElementKey LeftThigh(TEXT("thigh_l"), ERigElementType::Bone);
	const FRigElementKey RightThigh(TEXT("thigh_r"), ERigElementType::Bone);
	const FRigElementKey LeftCalf(TEXT("calf_l"), ERigElementType::Bone);
	const FRigElementKey RightCalf(TEXT("calf_r"), ERigElementType::Bone);
	const FRigElementKey LeftBall(TEXT("ball_l"), ERigElementType::Bone);
	const FRigElementKey RightBall(TEXT("ball_r"), ERigElementType::Bone);
	const FRigElementKey LeftContact(TEXT("FootPlant_L"), ERigElementType::Curve);
	const FRigElementKey RightContact(TEXT("FootPlant_R"), ERigElementType::Curve);

	constexpr float OffsetSmoothingTime = 0.09f;
	constexpr float ContactResponse = 18.0f;
	constexpr float PlantThreshold = 0.8f;
	constexpr double PinReleaseSpeed = 190.0;
	constexpr double MaxPinDistance = 20.0;

	struct FGroundQuery
	{
		const UWorld& World;
		const URigHierarchy& Hierarchy;
		const FTransform& RigToWorld;
		const FCollisionQueryParams& Params;
		ECollisionChannel Channel;
		double RootHeight;
		double ReferenceRootHeight;
		double Scale;
		double RootHeightDelta;
		float WalkableNormalZ;
		float DeltaTime;
		bool bMoving;
	};

	float SolveFoot(const FGroundQuery& Query, const FRigElementKey& Foot, const FRigElementKey& Ball,
		const FRigElementKey& ContactCurve, FStageShiftFootPlacementState& State, FTransform& Target)
	{
		Target = Query.Hierarchy.GetGlobalTransform(Foot);
		const FVector Ankle = Query.RigToWorld.TransformPosition(Target.GetLocation());
		const double ReferenceHeight = Query.Hierarchy.GetGlobalTransform(Foot, true).GetLocation().Z - Query.ReferenceRootHeight;
		const double Lift = (Ankle.Z - Query.RootHeight) / Query.Scale - ReferenceHeight;
		const bool bHasContact = Query.Hierarchy.IsCurveValueSet(ContactCurve);
		const float Contact = bHasContact
			? FMath::Clamp(Query.Hierarchy.GetCurveValue(ContactCurve), 0.0f, 1.0f)
			: static_cast<float>(FMath::Clamp((12.0 - Lift) / 10.0, 0.0, 1.0));

		const FVector BallLocation = Query.Hierarchy.Contains(Ball)
			? Query.RigToWorld.TransformPosition(Query.Hierarchy.GetGlobalTransform(Ball).GetLocation())
			: Ankle;
		const FVector Sample = Ankle + State.PlantOffset;
		FHitResult Hit;
		const bool bHit = Query.World.SweepSingleByChannel(Hit,
			Sample + FVector::UpVector * (28.0 * Query.Scale),
			Sample - FVector::UpVector * (62.0 * Query.Scale),
			FQuat::Identity, Query.Channel, FCollisionShape::MakeSphere(2.5 * Query.Scale), Query.Params);
		const bool bWalkable = bHit && !Hit.bStartPenetrating && Hit.ImpactNormal.Z >= Query.WalkableNormalZ;
		// Idle curves keep both contacts at one. Release the last gait's world-space
		// anchors on stopping so they cannot hold the idle pose in the previous stride.
		const float PinCurve = bWalkable && bHasContact && Query.bMoving ? Contact : 0.0f;
		const bool bStrongContact = PinCurve >= PlantThreshold;
		if (bStrongContact && !State.bPlanted)
		{
			// Replant at the current corrected position so a new contact cannot snap the foot.
			State.PlantLocation = BallLocation + State.PlantOffset;
		}
		State.bPlanted = bStrongContact;

		// Only raise the lock on strong contact. Keep the anchor through the curve's release.
		if (bStrongContact || PinCurve < State.PinAlpha)
		{
			State.PinAlpha = PinCurve;
		}
		FVector PinDelta = State.PlantLocation - BallLocation;
		PinDelta.Z = 0.0;
		const FVector DesiredPlantOffset = PinDelta.GetClampedToMaxSize(MaxPinDistance * Query.Scale) * State.PinAlpha;
		State.PlantOffset = bStrongContact ? DesiredPlantOffset
			: FMath::VInterpConstantTo(State.PlantOffset, DesiredPlantOffset, Query.DeltaTime, PinReleaseSpeed * Query.Scale);

		const double Height = bWalkable
			? FMath::Clamp(Hit.ImpactPoint.Z - Query.RootHeight, -22.0 * Query.Scale, 28.0 * Query.Scale)
			: 0.0;
		State.Height -= Query.RootHeightDelta * State.Weight;
		FMath::CriticallyDampedSmoothing(State.Height, State.HeightVelocity, Height, 0.0, Query.DeltaTime, OffsetSmoothingTime);
		State.Normal = FMath::VInterpTo(State.Normal, bWalkable ? Hit.ImpactNormal : FVector::UpVector,
			Query.DeltaTime, ContactResponse).GetSafeNormal();
		State.Weight = FMath::FInterpTo(State.Weight, bWalkable ? Contact : 0.0f, Query.DeltaTime, ContactResponse);

		const FVector WorldTarget = Ankle + State.PlantOffset + FVector::UpVector * State.Height;
		const FVector LocalUp = Query.RigToWorld.InverseTransformVectorNoScale(FVector::UpVector);
		const FVector LocalNormal = Query.RigToWorld.InverseTransformVectorNoScale(State.Normal);
		const FQuat Tilt = FQuat::FindBetweenNormals(LocalUp, LocalNormal);
		Target.SetLocation(Query.RigToWorld.InverseTransformPosition(WorldTarget));
		Target.SetRotation((Tilt * Target.GetRotation()).GetNormalized());

		// Keep the solver active until the planar correction has faded out.
		const float PinWeight = static_cast<float>(FMath::SmoothStep(0.0, Query.Scale, State.PlantOffset.Size2D()));
		return FMath::Max(State.Weight, PinWeight);
	}

	void ClampLegTarget(const URigHierarchy& Hierarchy, const FRigElementKey& Thigh, const FRigElementKey& Calf,
		const FRigElementKey& Foot, float PelvisOffset, FTransform& Target)
	{
		if (!Hierarchy.Contains(Thigh) || !Hierarchy.Contains(Calf))
		{
			return;
		}

		const FVector Hip = Hierarchy.GetGlobalTransform(Thigh).GetLocation();
		const FVector Knee = Hierarchy.GetGlobalTransform(Calf).GetLocation();
		const FVector Ankle = Hierarchy.GetGlobalTransform(Foot).GetLocation();
		const double LegLength = FVector::Distance(Hip, Knee) + FVector::Distance(Knee, Ankle);
		const FVector AdjustedHip = Hip + FVector::UpVector * PelvisOffset;
		const FVector Reach = Target.GetLocation() - AdjustedHip;
		Target.SetLocation(AdjustedHip + Reach.GetClampedToMaxSize(LegLength));
	}
}

FRigUnit_StageShiftFootPlacement_Execute()
{
	const URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	const UWorld* World = ExecuteContext.GetWorld();
	const AActor* Owner = ExecuteContext.GetOwningActor();
	const ACharacter* Character = Cast<ACharacter>(Owner);
	const FTransform& RigToWorld = ExecuteContext.GetToWorldSpaceTransform();
	const double Scale = RigToWorld.TransformVector(FVector::UpVector).Size();
	if (!World || !Hierarchy || !Hierarchy->Contains(RootBone) ||
		!Hierarchy->Contains(LeftFoot) || !Hierarchy->Contains(RightFoot) || Scale <= UE_SMALL_NUMBER ||
		(Character && !Character->GetCharacterMovement()->IsMovingOnGround()))
	{
		LeftState = {};
		RightState = {};
		LeftWeight = RightWeight = PelvisOffset = 0.0f;
		PelvisHeight = PelvisVelocity = 0.0;
		bHasPreviousRoot = false;
		return;
	}

	const FVector Root = RigToWorld.TransformPosition(Hierarchy->GetGlobalTransform(RootBone).GetLocation());
	const bool bTeleported = bHasPreviousRoot && FVector::DistSquared(PreviousRoot, Root) > FMath::Square(65.0 * Scale);
	if (bTeleported)
	{
		LeftState = {};
		RightState = {};
		PelvisHeight = PelvisVelocity = 0.0;
	}

	const double RootDelta = bHasPreviousRoot && !bTeleported ? Root.Z - PreviousRoot.Z : 0.0;
	PreviousRoot = Root;
	bHasPreviousRoot = true;

	const FCollisionQueryParams Params(SCENE_QUERY_STAT(StageShiftFootPlacement), false, Owner);
	const float DeltaTime = FMath::Clamp(ExecuteContext.GetDeltaTime(), 0.0f, 0.1f);
	const float WalkableNormalZ = FMath::Cos(FMath::DegreesToRadians(WalkableFloorAngle));
	const FGroundQuery Query{*World, *Hierarchy, RigToWorld, Params, TraceChannel, Root.Z,
		Hierarchy->GetGlobalTransform(RootBone, true).GetLocation().Z, Scale, RootDelta, WalkableNormalZ, DeltaTime,
		Owner && Owner->GetVelocity().SizeSquared2D() >= 1.0};

	LeftWeight = SolveFoot(Query, LeftFoot, LeftBall, LeftContact, LeftState, LeftTarget);
	RightWeight = SolveFoot(Query, RightFoot, RightBall, RightContact, RightState, RightTarget);

	const double LowestSupport = FMath::Min(LeftState.Height * LeftWeight, RightState.Height * RightWeight);
	const double PelvisTarget = FMath::Clamp(LowestSupport, -22.0 * Scale, 0.0);
	FMath::CriticallyDampedSmoothing(PelvisHeight, PelvisVelocity, PelvisTarget, 0.0, DeltaTime, OffsetSmoothingTime);
	PelvisOffset = static_cast<float>(PelvisHeight / Scale);

	ClampLegTarget(*Hierarchy, LeftThigh, LeftCalf, LeftFoot, PelvisOffset, LeftTarget);
	ClampLegTarget(*Hierarchy, RightThigh, RightCalf, RightFoot, PelvisOffset, RightTarget);
}
