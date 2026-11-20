#include "Stage/Actions/StageImpactAction.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/Physics/PhysicalMontageComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Physics/PhysicsActor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"
#include "StageInteractionHelpers.h"
#include "TimerManager.h"

void UStageImpactAction::Setup(EStageImpactMode InMode, AActor* InTarget, AStagePhysicsActor* InProp, float InDelay,
    float InStageTimeRequired, UAnimSequenceBase* InWindup, UAnimMontage* InFall)
{
	Mode = InMode;
	Target = InTarget;
	Prop = InProp;
	Delay = InDelay;
	StageTimeRequired = InStageTimeRequired;
	Windup = InWindup;
	Fall = InFall;
}

void UStageImpactAction::OnStart_Implementation()
{
	AActor* Owner = StageInteraction::GetOwner(Context);
	if (!IsValid(Owner) || !Target.IsValid()) return Fail(TEXT("Missing performer or target"));
	if (const auto* Manager = Context.Manager.Get(); StageTimeRequired > 0.0f && Manager && Manager->GetActiveStage())
		if (Manager->GetActiveStage()->MainDuration - Manager->GetStageElapsedTime() < StageTimeRequired)
			return Fail(TEXT("Too little stage time left"));

	if (Mode != EStageImpactMode::Collapse)
	{
		const FVector Heading = (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D();
		if (!Heading.IsNearlyZero()) Owner->SetActorRotation(Heading.Rotation());
		const auto* Character = Cast<ACharacter>(Owner);
		auto* Instance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
		if (Instance && Windup && Delay > 0.0f)
		{
			AnimInstance = Instance;
			PlayingWindup = Instance->PlaySlotAnimationAsDynamicMontage(Windup, TEXT("RightHand"), 0.07f, 0.1f,
			    FMath::Max(0.1f, Windup->GetPlayLength() / Delay));
		}
	}
	if (Delay > 0.0f) GetWorld()->GetTimerManager().SetTimer(ImpactTimer, this, &ThisClass::PerformImpact, Delay, false);
	else PerformImpact();
}

void UStageImpactAction::PerformImpact()
{
	AActor* Owner = StageInteraction::GetOwner(Context);
	AActor* Victim = Target.Get();
	if (!IsValid(Owner) || !IsValid(Victim)) return Fail(TEXT("Missing performer or target at impact"));
	if (Mode == EStageImpactMode::Collapse) return FinishHit();

	auto* Manipulation = Owner->FindComponentByClass<UPhysicsManipulationComponent>();
	auto* Item = Prop.Get();
	if (!StageInteraction::IsHolding(Manipulation, Item)) return Fail(TEXT("Performer no longer holds the required prop"));
	const FVector TargetPosition = Victim->GetActorLocation();
	if (Mode == EStageImpactMode::Strike)
	{
		constexpr float StrikeReach = 145.0f;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(StageStrike), false, Owner);
		Query.AddIgnoredActor(Item);
		Query.AddIgnoredActor(Victim);
		if (FVector::DistSquared(Owner->GetActorLocation(), TargetPosition) > FMath::Square(StrikeReach)
		    || GetWorld()->LineTraceTestByChannel(Owner->GetActorLocation(), TargetPosition, ECC_Visibility, Query))
			return Fail(TEXT("Strike target out of reach or blocked"));
		return FinishHit();
	}

	FInteractionContext Interaction;
	if (!StageInteraction::MakeContext(Owner, Item, {}, EInteractionPhase::End, 0.0f, Interaction)) return Fail(TEXT("Could not release prop"));
	UPrimitiveComponent* Body = Item->GetStaticMeshComponent();
	FlightBody = Body;
	if (const FBodyInstance* BodyInstance = Body->GetBodyInstance())
	{
		bRestoreHitNotifications = !BodyInstance->bNotifyRigidBodyCollision;
		bRestoreCCD = !BodyInstance->bUseCCD;
	}
	Body->SetNotifyRigidBodyCollision(true);
	Body->SetUseCCD(true);
	Body->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandlePropHit);
	Item->IgnoreCatchFor(Victim);
	Manipulation->SetThrowTarget(TargetPosition, FVector::ZeroVector);
	Interaction.HoldDuration = 1.0f;
	const EInteractionResult Result = IInteractionInterface::Execute_OnInteraction(Item, Interaction);
	Manipulation->ClearThrowTarget();
	if (Result != EInteractionResult::Completed || !Body->IsSimulatingPhysics()) return Fail(TEXT("Prop did not launch"));
	GetWorld()->GetTimerManager().SetTimer(FlightTimer, FTimerDelegate::CreateWeakLambda(this, [this]
	{
		Fail(TEXT("Thrown prop missed"));
	}), 1.3f, false);
}

void UStageImpactAction::HandlePropHit(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult&)
{
	if (OtherActor != Target.Get() || !GetWorld()->GetTimerManager().IsTimerActive(FlightTimer)) return;
	GetWorld()->GetTimerManager().ClearTimer(FlightTimer);
	FlightTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FinishHit);
}

EStageActionStopMode UStageImpactAction::GetStopMode_Implementation(EStageActionStopReason Reason) const
{
	if (Reason == EStageActionStopReason::Interrupt && FlightBody.IsValid()) return EStageActionStopMode::FinishAction;
	return Super::GetStopMode_Implementation(Reason);
}

void UStageImpactAction::FinishHit()
{
	AActor* Victim = Target.Get();
	auto* Physics = IsValid(Victim) ? Victim->FindComponentByClass<UPhysicalMontageComponent>() : nullptr;
	const AActor* Owner = StageInteraction::GetOwner(Context);
	const FVector Push = Mode != EStageImpactMode::Collapse && IsValid(Victim) && IsValid(Owner)
	    ? Victim->GetActorLocation() - Owner->GetActorLocation() : FVector::ZeroVector;
	if (!Physics || !Physics->StayDown(Fall, Push)) return Fail(TEXT("Target could not fall"));
	if (auto* Manager = Context.Manager.Get())
		Manager->AddDiagnostic(FString::Printf(TEXT("StageImpact / %s / target[%s]"), *UEnum::GetValueAsString(Mode), *Victim->GetActorNameOrLabel()));
	Finish(EStageActionResult::Succeeded);
}

void UStageImpactAction::Fail(const TCHAR* Reason)
{
	if (auto* Manager = Context.Manager.Get()) Manager->AddDiagnostic(FString(TEXT("StageImpact / ")) + Reason);
	Finish(EStageActionResult::Failed);
}

void UStageImpactAction::Cleanup_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ImpactTimer);
		World->GetTimerManager().ClearTimer(FlightTimer);
	}
	if (auto* Body = FlightBody.Get())
	{
		Body->OnComponentHit.RemoveDynamic(this, &ThisClass::HandlePropHit);
		if (bRestoreHitNotifications) Body->SetNotifyRigidBodyCollision(false);
		if (bRestoreCCD) Body->SetUseCCD(false);
		if (auto* Item = Prop.Get()) Item->IgnoreCatchFor(nullptr);
	}
	if (auto* Instance = AnimInstance.Get(); Instance && PlayingWindup.IsValid()) Instance->Montage_Stop(0.1f, PlayingWindup.Get());
}
