#include "Core/StageShiftCharacter.h"

#include "Animation/MeshDeformer.h"
#include "Animation/StageShiftAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"
#include "Components/Dialogue/DialogueComponent.h"
#include "Components/FacialAnimation/FacialAnimationComponent.h"
#include "Components/FacialAnimation/FacialAnimationProfile.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Components/Physics/CharacterPhysicsReactionComponent.h"
#include "Components/Physics/DollPhysicsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/StageShiftCharacterMovementComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OptimusDeformerInstance.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/ConstructorHelpers.h"

AStageShiftCharacter::AStageShiftCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UStageShiftCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	ConfigureMovementCollision();

	PhysicalMontageComponent = CreateDefaultSubobject<UPhysicalMontageComponent>("Physical Montage Component");
	PhysicsReactionComponent = CreateDefaultSubobject<UCharacterPhysicsReactionComponent>(TEXT("Physics Reaction Component"));
	DollPhysicsComponent = CreateDefaultSubobject<UDollPhysicsComponent>(TEXT("DollPhysicsComponent"));
	DollPhysicsComponent->SetupAttachment(GetCapsuleComponent());

	DialogueComponent = CreateDefaultSubobject<UDialogueComponent>(TEXT("DialogueComponent"));
	DialogueWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DialogueWidgetComponent"));
	FacialAnimationComponent = CreateDefaultSubobject<UFacialAnimationComponent>(TEXT("FacialAnimationComponent"));

	DialogueWidgetComponent->SetupAttachment(GetMesh());
	DialogueWidgetComponent->SetRelativeLocation(FVector(0.0, 0.0, 250.0));
	DialogueWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	DialogueWidgetComponent->SetDrawAtDesiredSize(true);
	DialogueWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DialogueComponent->SetWidgetComponentProperty(GET_MEMBER_NAME_CHECKED(AStageShiftCharacter, DialogueWidgetComponent));

	static ConstructorHelpers::FClassFinder<UDialogueBubbleWidget> DialogueWidgetClass(TEXT("/Game/DEV/UI/Dialogue/W_DialogueBubble"));

	if (DialogueWidgetClass.Succeeded())
	{
		DialogueWidgetComponent->SetWidgetClass(DialogueWidgetClass.Class);
	}

	static ConstructorHelpers::FObjectFinder<UFacialAnimationProfile> FacialProfile(TEXT("/Game/MODELS/Character/FacialAnimation/DA_Character_FacialAnimation"));

	if (FacialProfile.Succeeded())
	{
		FacialAnimationComponent->SetProfile(FacialProfile.Object);
	}

	HairMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HairMesh"));
	HatMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HatMesh"));
	OutfitMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("OutfitMesh"));
	for (USkeletalMeshComponent* Part : {HairMesh.Get(), HatMesh.Get(), OutfitMesh.Get()})
	{
		Part->SetupAttachment(GetMesh());
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AStageShiftCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	for (USkeletalMeshComponent* Part : {HairMesh.Get(), HatMesh.Get(), OutfitMesh.Get()})
	{
		Part->SetLeaderPoseComponent(GetMesh());
	}
}

void AStageShiftCharacter::ConfigureMovementCollision()
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);

	USkeletalMeshComponent* MeshComponent = GetMesh();
	MeshComponent->SetCollisionProfileName(TEXT("CharacterMesh"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	GetCharacterMovement()->bEnablePhysicsInteraction = false;
}

void AStageShiftCharacter::BeginPlay()
{
	ConfigureMovementCollision();
	Super::BeginPlay();

	if (DollPressDeformer)
	{
		for (USkeletalMeshComponent* Part : {GetMesh(), HairMesh.Get(), HatMesh.Get(), OutfitMesh.Get()})
		{
			Part->SetMeshDeformer(DollPressDeformer);
		}
		DollPressHandle = GetMesh()->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &ThisClass::UpdateDollPress));
	}
}

void AStageShiftCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DollPressHandle.IsValid())
	{
		GetMesh()->UnregisterOnBoneTransformsFinalizedDelegate(DollPressHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void AStageShiftCharacter::UpdateDollPress()
{
	const auto* Animation = Cast<UStageShiftAnimInstance>(GetMesh()->GetAnimInstance());
	const FStageShiftDollPose EmptyPose;
	const FStageShiftDollPose& Motion = Animation ? Animation->GetDollPose() : EmptyPose;
	const bool bPhysical = IsDollPhysicsActive();
	const TConstArrayView<FStageShiftDollContact> Contacts = bPhysical ? DollPhysicsComponent->GetContacts() : TConstArrayView<FStageShiftDollContact>();
	const FVector SquashOrigin = GetMesh()->GetBoneLocation(TEXT("pelvis"));
	static const FName Centers[] = {TEXT("PressCenter"), TEXT("PressCenter1"), TEXT("PressCenter2"), TEXT("PressCenter3")};
	static const FName Directions[] = {TEXT("PressDirection"), TEXT("PressDirection1"), TEXT("PressDirection2"), TEXT("PressDirection3")};
	static const FName Depths[] = {TEXT("PressDepth"), TEXT("PressDepth1"), TEXT("PressDepth2"), TEXT("PressDepth3")};
	static const FName Radii[] = {TEXT("PressRadius"), TEXT("PressRadius1"), TEXT("PressRadius2"), TEXT("PressRadius3")};

	for (USkeletalMeshComponent* Part : {GetMesh(), HairMesh.Get(), HatMesh.Get(), OutfitMesh.Get()})
	{
		auto* Deformer = Cast<UOptimusDeformerInstance>(Part->GetMeshDeformerInstance());
		if (!Deformer) continue;

		const FTransform& Transform = Part->GetComponentTransform();
		const double Scale = FMath::Max(Transform.GetScale3D().GetAbsMax(), UE_SMALL_NUMBER);
		Deformer->SetVectorVariable(TEXT("SquashOrigin"), Transform.InverseTransformPosition(SquashOrigin));
		Deformer->SetVectorVariable(TEXT("SquashAxis"), Transform.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal());
		Deformer->SetFloatVariable(TEXT("SquashAmount"), bPhysical ? FMath::Clamp(Motion.Compression * 2.5f, -0.04f, 0.12f) : 0.0f);
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Centers); ++Index)
		{
			float Depth = 0.0f;
			FVector Center = FVector::ZeroVector;
			FVector Normal = FVector::ZeroVector;
			if (Contacts.IsValidIndex(Index))
			{
				const FStageShiftDollContact& Contact = Contacts[Index];
				Depth = Contact.Depth;
				Center = GetMesh()->GetBoneTransform(GetMesh()->GetBoneIndex(Contact.Bone)).TransformPosition(Contact.LocalPoint);
				Normal = Contact.Normal;
			}
			else if (!bPhysical && Index == 0)
			{
				Depth = FMath::Max(Motion.ContactOffset, 0.0f);
				Center = GetMesh()->GetSocketTransform(Motion.ContactBone).TransformPosition(Motion.ContactAnchor);
				Normal = Motion.ContactNormal;
			}
			Deformer->SetFloatVariable(Depths[Index], Depth / Scale);
			if (Depth > 0.0f)
			{
				Deformer->SetVectorVariable(Centers[Index], Transform.InverseTransformPosition(Center));
				Deformer->SetVectorVariable(Directions[Index], Transform.InverseTransformVectorNoScale(Normal));
				Deformer->SetFloatVariable(Radii[Index], 17.0 / Scale);
			}
		}
	}
}

void AStageShiftCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bStumblePending)
	{
		bStumblePending = false;
		PhysicsReactionComponent->PlayReaction(PendingPhysicsReaction, StumbleDirection, PendingPhysicsInput);
	}
}

void AStageShiftCharacter::Landed(const FHitResult& Hit)
{
	const float ImpactSpeed = FMath::Max(-GetVelocity().Z, 0.0);
	Super::Landed(Hit);
	if (!PhysicalMontageComponent->IsPhysicsMontageActive())
	{
		if (UStageShiftAnimInstance* Animation = Cast<UStageShiftAnimInstance>(GetMesh()->GetAnimInstance()))
		{
			Animation->AddLandingSquash(LandingSquash * FMath::Clamp(ImpactSpeed / 600.0f, 0.0f, 1.5f));
		}
	}
}

void AStageShiftCharacter::MoveBlockedBy(const FHitResult& Impact)
{
	Super::MoveBlockedBy(Impact);

	if (PhysicalMontageComponent->IsPhysicsMontageActive() || !GetCharacterMovement()->IsMovingOnGround() || !Impact.IsValidBlockingHit()) return;

	UPrimitiveComponent* Obstacle = Impact.GetComponent();
	if (!Obstacle || Obstacle == GetHeldPhysicsComponent()) return;

	if (FMath::Abs(Impact.ImpactNormal.Z) < 0.65)
	{
		ApplyDollContact(Impact);
	}

	ConsiderStumble(Impact);
}

void AStageShiftCharacter::ConsiderPhysicsHit(const FHitResult& Impact, const FVector& NormalImpulse)
{
	UPrimitiveComponent* Other = Impact.GetComponent();

	if (!bEnablePhysicsHitKnockdown || bStumblePending || IsPhysicsMontageActive() || !Other ||
		Other->GetOwner() == this || Other == GetHeldPhysicsComponent() || !Other->IsSimulatingPhysics(Impact.BoneName)) return;

	const FBodyInstance* OtherBody = Other->GetBodyInstance(Impact.BoneName);
	const FBodyInstance* HitBody = GetMesh()->GetBodyInstance(Impact.MyBoneName);

	if (!OtherBody || !HitBody) return;

	const double OtherMass = FMath::Max(OtherBody->GetBodyMass(), 0.1f);

	if (OtherMass < FMath::Max(PhysicsHitMinMass, 0.0f)) return;

	const FVector Normal = Impact.ImpactNormal.GetSafeNormal();

	if (Normal.IsNearlyZero()) return;

	const double Impulse = FMath::Abs(FVector::DotProduct(NormalImpulse, Normal));
	if (Impulse < FMath::Max(PhysicsHitMinImpulse, 0.0f) * 100.0) return;

	const double IncomingSpeed = FVector::DotProduct(Other->GetPhysicsLinearVelocityAtPoint(Impact.ImpactPoint, Impact.BoneName), Normal) + Impulse / OtherMass;
	const double OwnSpeedBefore = FVector::DotProduct(GetMesh()->GetPhysicsLinearVelocityAtPoint(Impact.ImpactPoint, Impact.MyBoneName), Normal) - Impulse / FMath::Max(HitBody->GetBodyMass(), 0.1f);
	const double ClosingSpeed = IncomingSpeed - OwnSpeedBefore;
	const float Threshold = FMath::Max(PhysicsHitMinSpeed, 1.0f);

	if (IncomingSpeed < Threshold || ClosingSpeed < Threshold) return;

	const double CharacterMass = FMath::Max(GetMesh()->GetMass(), 1.0f);
	const double ExtraImpulse = FMath::Clamp(Impulse * FMath::Max(PhysicsHitKnockbackScale, 0.0f), 0.0,
		CharacterMass * FMath::Max(PhysicsHitMaxKnockbackSpeed, 0.0f));
	PendingPhysicsInput = FPhysicalAnimationInput();
	PendingPhysicsInput.InitialVelocity = GetVelocity();
	PendingPhysicsInput.Impulse = Normal * ExtraImpulse;
	PendingPhysicsInput.ImpulseLocation = Impact.ImpactPoint;
	PendingPhysicsInput.ImpulseBone = Impact.MyBoneName;
	StumbleDirection = Normal;
	PendingPhysicsReaction = ECharacterPhysicsReaction::Impact;
	bStumblePending = true;
}

void AStageShiftCharacter::ConsiderStumble(const FHitResult& Impact)
{
	UPrimitiveComponent* Obstacle = Impact.GetComponent();
	if (bStumblePending || PhysicalMontageComponent->IsPhysicsMontageActive() || !GetCharacterMovement()->IsMovingOnGround() ||
		!Obstacle || Obstacle == GetHeldPhysicsComponent() || !Obstacle->IsSimulatingPhysics(Impact.BoneName)) return;
	const FVector Velocity = GetVelocity();
	if (Velocity.SizeSquared2D() < FMath::Square(150.0)) return;

	const FVector ToHit = (Impact.ImpactPoint - GetActorLocation()).GetSafeNormal2D();
	if (FVector::DotProduct(Velocity.GetSafeNormal2D(), ToHit) < 0.45) return;

	const float KneeZ = FMath::Min(GetMesh()->GetSocketLocation(TEXT("calf_l")).Z, GetMesh()->GetSocketLocation(TEXT("calf_r")).Z);
	if (Obstacle->Bounds.GetBox().Max.Z > KneeZ - 5.0f || Impact.ImpactPoint.Z > KneeZ) return;

	PendingPhysicsInput = FPhysicalAnimationInput();
	PendingPhysicsInput.InitialVelocity = FVector(Velocity.X, Velocity.Y, 0.0);
	StumbleDirection = Velocity;
	PendingPhysicsReaction = ECharacterPhysicsReaction::Stumble;
	bStumblePending = true;
}

void AStageShiftCharacter::ApplyDollContact(const FHitResult& Impact)
{
	if (IsDollPhysicsActive()) return;
	auto* Animation = Cast<UStageShiftAnimInstance>(GetMesh()->GetAnimInstance());
	if (!Animation) return;

	UPrimitiveComponent* Obstacle = Impact.GetComponent();
	const FVector Away = Impact.ImpactNormal.GetSafeNormal2D();
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bNewContact = LastSquashObstacle != Obstacle || Now - LastSquashContactTime > 0.18;
	LastSquashObstacle = Obstacle;
	LastSquashContactTime = Now;

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float ImpactSpeed = -FVector::DotProduct(GetVelocity(), Away);
	const float Push = FMath::Clamp(-FVector::DotProduct(Movement->GetCurrentAcceleration(), Away)
		/ FMath::Max(Movement->GetMaxAcceleration(), 1.0f), 0.0, 1.0);
	Animation->UpdateContactSquash(Away, Impact.ImpactPoint, ImpactSpeed, Push, bNewContact, Impact.MyBoneName);
}

bool AStageShiftCharacter::IsPhysicsMontageActive() const
{
	return PhysicalMontageComponent->IsPhysicsMontageActive();
}

bool AStageShiftCharacter::IsDollPhysicsActive() const
{
	return DollPhysicsComponent && DollPhysicsComponent->IsDriving();
}
