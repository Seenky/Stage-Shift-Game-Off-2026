#include "Components/Physics/CharacterPhysicsReactionComponent.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "PhysicsEngine/BodyInstance.h"

namespace
{
	FTransform ReadBoneTransform(const USkeletalMeshComponent& Mesh, FName Bone)
	{
		const FBodyInstance* Body = Mesh.GetBodyInstance(Bone);
		return Body && Body->IsValidBodyInstance() ? Body->GetUnrealWorldTransform() : Mesh.GetSocketTransform(Bone);
	}

	bool IsPlayable(const FPhysicalMontageSettings& Settings)
	{
		return Settings.Montage || Settings.bAllowWithoutAnimation;
	}
}

void UCharacterPhysicsReactionComponent::BeginPlay()
{
	Super::BeginPlay();
	Executor = GetOwner()->FindComponentByClass<UPhysicalMontageComponent>();
	if (Executor.IsValid())
	{
		StartedHandle = Executor->OnPhysicsAnimationStarted.AddUObject(this, &ThisClass::CaptureRecoveryFrame);
		if (!Executor->RecoverySelector.IsBound()) Executor->RecoverySelector.BindUObject(this, &ThisClass::SelectRecovery);
	}
}

void UCharacterPhysicsReactionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Executor.IsValid())
	{
		Executor->OnPhysicsAnimationStarted.Remove(StartedHandle);
		if (Executor->RecoverySelector.IsBoundToObject(this)) Executor->RecoverySelector.Unbind();
	}
	Super::EndPlay(Reason);
}

const UCharacterPhysicsAnimationSet* UCharacterPhysicsReactionComponent::GetAnimationSet() const
{
	return AnimationSet ? AnimationSet.Get() : GetDefault<UCharacterPhysicsAnimationSet>();
}

bool UCharacterPhysicsReactionComponent::PlayReaction(ECharacterPhysicsReaction Reaction, const FVector& WorldDirection, const FPhysicalAnimationInput& Input)
{
	if (!Executor.IsValid() || Executor->IsPhysicsMontageActive()) return false;
	const UCharacterPhysicsAnimationSet* Data = GetAnimationSet();
	const FPhysicalMontageSettings* Settings = &Data->FallbackAnimation;
	if (const FCharacterPhysicsReactionAnimation* Entry = Data->Reactions.Find(Reaction))
	{
		if (IsPlayable(Entry->DefaultAnimation)) Settings = &Entry->DefaultAnimation;
		const FVector Direction = WorldDirection.GetSafeNormal2D(UE_SMALL_NUMBER, GetOwner()->GetActorForwardVector());
		const FVector Local = GetOwner()->GetActorQuat().UnrotateVector(Direction);
		const ECharacterFallDirection Side = FMath::Abs(Local.X) >= FMath::Abs(Local.Y)
			? (Local.X >= 0.0 ? ECharacterFallDirection::Forward : ECharacterFallDirection::Backward)
			: (Local.Y >= 0.0 ? ECharacterFallDirection::Right : ECharacterFallDirection::Left);
		if (const FPhysicalMontageSettings* Directional = Entry->DirectionalAnimations.Find(Side); Directional && IsPlayable(*Directional))
		{
			Settings = Directional;
		}
	}
	if (!IsPlayable(*Settings)) return false;
	if (UInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>()) Interaction->CancelInteraction();
	return Executor->PlayPhysicsAnimation(*Settings, Input);
}

void UCharacterPhysicsReactionComponent::CaptureRecoveryFrame()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	const FName Bone = GetAnimationSet()->OrientationBone;
	bHasRecoveryFrame = Mesh && !Bone.IsNone() && Mesh->DoesSocketExist(Bone);
	if (bHasRecoveryFrame)
	{
		LocalFacingDirection = ReadBoneTransform(*Mesh, Bone).InverseTransformVectorNoScale(Character->GetActorForwardVector());
	}
}

FPhysicsRecoverySelection UCharacterPhysicsReactionComponent::SelectRecovery(const USkeletalMeshComponent* Mesh) const
{
	FPhysicsRecoverySelection Selection;
	const UCharacterPhysicsAnimationSet* Data = GetAnimationSet();
	if (!Mesh || Data->OrientationBone.IsNone() || !Mesh->DoesSocketExist(Data->OrientationBone)) return Selection;
	const FTransform Body = ReadBoneTransform(*Mesh, Data->OrientationBone);
	if (bHasRecoveryFrame)
	{
		const ECharacterRecoveryPose Pose = Body.TransformVectorNoScale(LocalFacingDirection).Z > 0.0
			? ECharacterRecoveryPose::Supine : ECharacterRecoveryPose::Prone;
		if (const TObjectPtr<UAnimMontage>* Montage = Data->RecoveryAnimations.Find(Pose)) Selection.Montage = Montage->Get();
	}
	if (!Data->HeadingBone.IsNone() && Mesh->DoesSocketExist(Data->HeadingBone))
	{
		Selection.FacingDirection = (ReadBoneTransform(*Mesh, Data->HeadingBone).GetLocation() - Body.GetLocation()).GetSafeNormal2D();
	}
	return Selection;
}
