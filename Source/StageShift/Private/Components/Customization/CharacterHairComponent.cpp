#include "Components/Customization/CharacterHairComponent.h"

#include "Animation/AnimClassInterface.h"
#include "Animation/Skeleton.h"
#include "Components/Customization/CharacterHairStyle.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Animation/StageShiftHairAnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterHair, Log, All);

UCharacterHairComponent::UCharacterHairComponent()
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	bUseBoundsFromLeaderPoseComponent = false;
	bUseAttachParentBound = false;
}

void UCharacterHairComponent::OnRegister()
{
	Super::OnRegister();
	RefreshHairstyle();
}

void UCharacterHairComponent::SetHairstyle(UCharacterHairStyle* NewHairstyle)
{
	Hairstyle = NewHairstyle;
	RefreshHairstyle();
}

void UCharacterHairComponent::RefreshHairstyle()
{
	if (IsTemplate()) return;
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Body || Body == this) return;

	HandleExistingParallelEvaluationTask(true, true);
	SetLeaderPoseComponent(nullptr);
	SetAnimInstanceClass(nullptr);
	ClearMorphTargets();
	bHairPhysicsEnabled = Hairstyle && Hairstyle->bEnablePhysics;
	USkeletalMesh* Mesh = Hairstyle ? Hairstyle->Mesh.Get() : nullptr;
	if (Mesh && !Body->DoesSocketExist(Hairstyle->AttachmentSocket))
	{
		UE_LOG(LogCharacterHair, Warning, TEXT("%s: missing attachment %s on %s; hiding hair."),
			*GetNameSafe(GetOwner()), *Hairstyle->AttachmentSocket.ToString(), *GetNameSafe(Body->GetSkeletalMeshAsset()));
		Mesh = nullptr;
	}
	SetSkeletalMesh(Mesh, true);
	if (!Mesh)
	{
		SetComponentTickEnabled(false);
		return;
	}

	AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Hairstyle->AttachmentSocket);
	SetRelativeTransform(Hairstyle->AttachmentTransform);
	AddTickPrerequisiteComponent(Body);

	UClass* Class = Hairstyle->AnimationClass.Get();
	const IAnimClassInterface* Interface = IAnimClassInterface::GetFromClass(Class);
	const USkeleton* TargetSkeleton = Interface ? Interface->GetTargetSkeleton() : nullptr;
	if (TargetSkeleton && !TargetSkeleton->IsCompatibleMesh(Mesh, false))
	{
		UE_LOG(LogCharacterHair, Warning, TEXT("%s: animation class %s is incompatible with %s; using the hair rest pose."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Class), *GetNameSafe(Mesh));
		Class = nullptr;
	}
	SetAnimInstanceClass(Class);
	if (!Class) InitAnim(true);
	SetComponentTickEnabled(Class != nullptr);
	ResetHairSimulation();
}

void UCharacterHairComponent::SetHairPhysicsEnabled(bool bEnabled)
{
	if (bHairPhysicsEnabled == bEnabled) return;
	HandleExistingParallelEvaluationTask(true, true);
	bHairPhysicsEnabled = bEnabled;
	// Rebuild only this component's stock AnimDynamics nodes with the new settings.
	InitAnim(true);
	ResetHairSimulation();
}

void UCharacterHairComponent::ResetHairSimulation()
{
	ResetAnimInstanceDynamics(ETeleportType::ResetPhysics);
}
