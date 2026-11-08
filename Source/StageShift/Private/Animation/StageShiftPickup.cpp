#include "Animation/StageShiftPickup.h"

#include "Animation/StageShiftAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StageShiftPickup)

FRigUnit_StageShiftPickup_Execute()
{
	const ACharacter* Character = Cast<ACharacter>(ExecuteContext.GetOwningActor());
	const UStageShiftAnimInstance* Animation = Character && Character->GetMesh()
		? Cast<UStageShiftAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (!Hierarchy || !Animation || Animation->PickupAlpha <= KINDA_SMALL_NUMBER) return;

	const FName Bones[] = {TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r")};
	for (FName Bone : Bones)
	{
		if (!Hierarchy->Contains(FRigElementKey(Bone, ERigElementType::Bone))) return;
	}

	Solver.Root = TEXT("pelvis");
	Solver.Settings.Iterations = 50;
	Solver.Settings.SubIterations = 4;
	Solver.Settings.RootBehavior = EPBIKRootBehavior::PinToInput;
	Solver.Settings.bAllowStretch = false;
	Solver.BoneSettings = BoneSettings;
	Solver.Effectors.SetNum(3);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FPBIKEffector& Effector = Solver.Effectors[Index];
		Effector.Bone = Bones[Index];
		Effector.Transform = Hierarchy->GetGlobalTransform(FRigElementKey(Bones[Index], ERigElementType::Bone));
		Effector.PositionAlpha = 1.0f;
		Effector.RotationAlpha = 1.0f;
		Effector.PinRotation = 1.0f;
	}
	// Feet retain their incoming contact points; only the hand follows the prop.
	const FVector Target = ExecuteContext.GetToWorldSpaceTransform().InverseTransformPosition(Animation->PickupTarget);
	FPBIKEffector& Hand = Solver.Effectors[0];
	// CarryPoint may be attached to a socket offset from the wrist bone.
	const FVector WristTarget = Target - Hand.Transform.TransformVector(Animation->PickupHandOffset);
	Hand.Transform.SetLocation(FMath::Lerp(Hand.Transform.GetLocation(), WristTarget, Animation->PickupAlpha));
	const FRigElementKey Pelvis(TEXT("pelvis"), ERigElementType::Bone);
	FTransform Hip = Hierarchy->GetGlobalTransform(Pelvis);
	const float FloorHeight = FMath::Min(Solver.Effectors[1].Transform.GetLocation().Z, Solver.Effectors[2].Transform.GetLocation().Z);
	const float LegHeight = Hip.GetLocation().Z - FloorHeight;
	const float LowReach = FMath::Clamp((FloorHeight + LegHeight * 0.8f - Target.Z) / FMath::Max(LegHeight * 0.65f, 1.0f), 0.0f, 1.0f);
	Hip.AddToTranslation(FVector(0, 0, -LegHeight * 0.4f * LowReach * Animation->PickupAlpha));
	Hierarchy->SetGlobalTransform(Pelvis, Hip, false, true);
	Solver.Execute(ExecuteContext);
}
