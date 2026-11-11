#include "Animation/AnimNode_CharacterHairDynamics.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/StageShiftHairAnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterHairDynamics, Log, All);

void FAnimNode_CharacterHairDynamics::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Dynamics.Reset();
	ToComponent.LocalPose = SourcePose;
	const auto* Instance = Cast<UStageShiftHairAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	const FBoneContainer& RequiredBones = Context.AnimInstanceProxy->GetRequiredBones();
	TSet<int32> SimulatedBones;
	if (Instance && Instance->PhysicsSettings.Alpha > 0.0f && RequiredBones.IsValid())
	{
		const FReferenceSkeleton& Skeleton = RequiredBones.GetReferenceSkeleton();
		Dynamics.Reserve(Instance->Strands.Num());
		for (const FCharacterHairStrand& Strand : Instance->Strands)
		{
			const int32 Start = Skeleton.FindBoneIndex(Strand.BoundBone);
			const int32 End = Strand.ChainEnd.IsNone() ? Start : Skeleton.FindBoneIndex(Strand.ChainEnd);
			TArray<int32> Bones;
			for (int32 Bone = End; Bone != INDEX_NONE; Bone = Skeleton.GetParentIndex(Bone))
			{
				Bones.Insert(Bone, 0);
				if (Bone == Start) break;
			}
			if (Start <= 0 || Bones.IsEmpty() || Bones[0] != Start ||
				Bones.ContainsByPredicate([&](int32 Bone) { return SimulatedBones.Contains(Bone); }))
			{
				UE_LOG(LogCharacterHairDynamics, Warning, TEXT("Skipping invalid or overlapping hair strand %s -> %s on %s."),
					*Strand.BoundBone.ToString(), *Strand.ChainEnd.ToString(), *Skeleton.GetBoneName(0).ToString());
				continue;
			}

			const FCharacterHairPhysicsSettings& Settings = Instance->PhysicsSettings;
			FAnimNode_AnimDynamics& Node = Dynamics.AddDefaulted_GetRef();
			Node.BoundBone.BoneName = Strand.BoundBone;
			Node.ChainEnd.BoneName = Strand.ChainEnd;
			Node.bChain = Bones.Num() > 1;
			Node.SimulationSpace = AnimPhysSimSpaceType::Component;
			Node.Alpha = FMath::Clamp(Settings.Alpha, 0.0f, 1.0f);
			Node.LODThreshold = Settings.SimulationLODThreshold;
			Node.bOverrideLinearDamping = true;
			Node.LinearDampingOverride = FMath::Clamp(Settings.LinearDamping, 0.7f, 1.0f);
			Node.bOverrideAngularDamping = true;
			Node.AngularDampingOverride = FMath::Clamp(Settings.AngularDamping, 0.7f, 1.0f);
			Node.bAngularSpring = true;
			Node.AngularSpringConstant = FMath::Max(Settings.AngularSpringConstant, 0.0f);
			Node.bLinearSpring = false;
			Node.GravityScale = FMath::Max(Settings.GravityScale, 0.0f);
			Node.bEnableWind = false;
			Node.ComponentLinearAccScale = FVector(FMath::Clamp(Settings.LinearInertiaScale, 0.0f, 1.0f));
			Node.ComponentLinearVelScale = FVector::ZeroVector;
			Node.ComponentAppliedLinearAccClamp = FVector(FMath::Max(Settings.MaxLinearAcceleration, 0.0f));
			Node.SimSpaceSettings.SimSpaceAngularAlpha = FMath::Clamp(Settings.AngularInertiaScale, 0.0f, 1.0f);
			Node.SimSpaceSettings.MaxAngularVelocity = FMath::Max(Settings.MaxAngularVelocity, 0.0f);
			Node.SimSpaceSettings.MaxAngularAcceleration = FMath::Max(Settings.MaxAngularAcceleration, 0.0f);
			Node.NumSolverIterationsPreUpdate = 8;
			Node.NumSolverIterationsPostUpdate = 2;
			if (Settings.ScalpRadius > 0.0f)
			{
				Node.bUseSphericalLimits = true;
				FAnimPhysSphericalLimit& Limit = Node.SphericalLimits.AddDefaulted_GetRef();
				Limit.SphereLocalOffset = Settings.ScalpOffset;
				Limit.LimitRadius = Settings.ScalpRadius;
				Limit.LimitType = ESphericalLimitType::Outer;
			}

			Node.PhysicsBodyDefinitions.Reset(Bones.Num());
			for (int32 Bone : Bones)
			{
				SimulatedBones.Add(Bone);
				FAnimPhysBodyDefinition& Body = Node.PhysicsBodyDefinitions.AddDefaulted_GetRef();
				Body.BoundBone.BoneName = Skeleton.GetBoneName(Bone);
				Body.BoxExtents = Strand.BoxExtents.ComponentMax(FVector(1.0));
				Body.LocalJointOffset = Strand.LocalJointOffset;
				FAnimPhysConstraintSetup& Limits = Body.ConstraintSetup;
				Limits.AngularLimitsMax = FVector(FMath::Clamp(Strand.SwingLimit, 0.0f, 45.0f));
				const int32 Axis = FMath::Clamp(static_cast<int32>(Strand.BoneAxis), 0, 2);
				Limits.AngularLimitsMax[Axis] = FMath::Clamp(Strand.TwistLimit, 0.0f, 45.0f);
				Limits.AngularLimitsMin = -Limits.AngularLimitsMax;
				Limits.TwistAxis = static_cast<AnimPhysTwistAxis>(Axis);
				Limits.AngularTargetAxis = Limits.TwistAxis;
				Limits.AngularTarget[Axis] = 1.0;
			}
		}
	}

	FAnimNode_Base* Previous = &ToComponent;
	for (FAnimNode_AnimDynamics& Node : Dynamics)
	{
		Node.ComponentPose.SetLinkNode(Previous);
		Previous = &Node;
	}
	ToLocal.ComponentPose.SetLinkNode(Previous);
	ToLocal.Initialize_AnyThread(Context);
}

void FAnimNode_CharacterHairDynamics::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	ToLocal.CacheBones_AnyThread(Context);
}

void FAnimNode_CharacterHairDynamics::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ToLocal.Update_AnyThread(Context);
}

void FAnimNode_CharacterHairDynamics::Evaluate_AnyThread(FPoseContext& Output)
{
	ToLocal.Evaluate_AnyThread(Output);
}

void FAnimNode_CharacterHairDynamics::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("Character Hair Dynamics (%d strands)"), Dynamics.Num()));
	ToLocal.GatherDebugData(DebugData);
}

void FAnimNode_CharacterHairDynamics::PreUpdate(const UAnimInstance* Instance)
{
	for (FAnimNode_AnimDynamics& Node : Dynamics) Node.PreUpdate(Instance);
}

void FAnimNode_CharacterHairDynamics::ResetDynamics(ETeleportType Type)
{
	for (FAnimNode_AnimDynamics& Node : Dynamics) Node.ResetDynamics(Type);
}
