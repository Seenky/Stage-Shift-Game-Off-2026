#include "Core/StageShiftCharacter.h"

#include "Animation/MeshDeformer.h"
#include "Animation/StageShiftAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"
#include "Components/Dialogue/DialogueComponent.h"
#include "Components/FacialAnimation/FacialAnimationComponent.h"
#include "Components/FacialAnimation/FacialAnimationProfile.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/StageShiftCharacterMovementComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "OptimusDeformerInstance.h"
#include "UObject/ConstructorHelpers.h"

AStageShiftCharacter::AStageShiftCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UStageShiftCharacterMovementComponent>(CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	ConfigureMovementCollision();

	PhysicalMontageComponent = CreateDefaultSubobject<UPhysicalMontageComponent>("Physical Montage Component");

	DialogueComponent = CreateDefaultSubobject<UDialogueComponent>(TEXT("DialogueComponent"));
	DialogueWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DialogueWidgetComponent"));
	FacialAnimationComponent = CreateDefaultSubobject<UFacialAnimationComponent>(TEXT("FacialAnimationComponent"));

	DialogueWidgetComponent->SetupAttachment(GetCapsuleComponent());
	DialogueWidgetComponent->SetRelativeLocation(FVector(0.0, 0.0, 142.0));
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
		DollPressHandle = GetMesh()->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &ThisClass::UpdateDollPress));
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
	const float Depth = FMath::Max(Motion.ContactOffset, 0.0f);
	const FVector Center = Depth > 0.0f
		? GetMesh()->GetSocketTransform(Motion.ContactBone).TransformPosition(Motion.ContactAnchor)
		: FVector::ZeroVector;

	for (USkeletalMeshComponent* Part : {GetMesh(), HairMesh.Get(), HatMesh.Get(), OutfitMesh.Get()})
	{
		auto* Deformer = Cast<UOptimusDeformerInstance>(Part->GetMeshDeformerInstance());
		if (!Deformer) continue;

		const FTransform& Transform = Part->GetComponentTransform();
		const double Scale = FMath::Max(Transform.GetScale3D().GetAbsMax(), UE_SMALL_NUMBER);
		Deformer->SetFloatVariable(TEXT("PressDepth"), Depth / Scale);
		if (Depth > 0.0f)
		{
			Deformer->SetVectorVariable(TEXT("PressCenter"), Transform.InverseTransformPosition(Center));
			Deformer->SetVectorVariable(TEXT("PressDirection"), Transform.InverseTransformVectorNoScale(Motion.ContactNormal));
			Deformer->SetFloatVariable(TEXT("PressRadius"), 17.0 / Scale);
		}
	}
}

void AStageShiftCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bStumblePending)
	{
		bStumblePending = false;
		PhysicalMontageComponent->StartPhysicsMontage(TEXT("Fall"), StumbleVelocity);
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

	const FVector Velocity = GetVelocity();
	if (FMath::Abs(Impact.ImpactNormal.Z) < 0.65)
	{
		ApplyDollContact(Impact);
	}

	if (bStumblePending || !Obstacle->IsSimulatingPhysics(Impact.BoneName)) return;
	if (Velocity.SizeSquared2D() < FMath::Square(150.0)) return;

	const FVector ToHit = (Impact.ImpactPoint - GetActorLocation()).GetSafeNormal2D();
	if (FVector::DotProduct(Velocity.GetSafeNormal2D(), ToHit) < 0.45) return;

	const float KneeZ = FMath::Min(GetMesh()->GetSocketLocation(TEXT("calf_l")).Z, GetMesh()->GetSocketLocation(TEXT("calf_r")).Z);
	if (Obstacle->Bounds.GetBox().Max.Z > KneeZ - 5.0f || Impact.ImpactPoint.Z > KneeZ) return;

	StumbleVelocity = FVector(Velocity.X, Velocity.Y, 0.0);
	bStumblePending = true;
}

void AStageShiftCharacter::ApplyDollContact(const FHitResult& Impact)
{
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
