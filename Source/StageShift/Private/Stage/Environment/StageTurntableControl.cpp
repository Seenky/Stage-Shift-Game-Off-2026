#include "Stage/Environment/StageTurntableControl.h"

#include "Components/StaticMeshComponent.h"
#include "Components/Interaction/InteractionComponent.h"
#include "Engine/StaticMesh.h"
#include "Stage/Environment/StageControlLeverComponent.h"
#include "Stage/Environment/StageTurntable.h"
#include "UObject/ConstructorHelpers.h"

AStageTurntableControl::AStageTurntableControl()
{
	PrimaryActorTick.bCanEverTick = false;
	ControlMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ControlMesh"));
	SetRootComponent(ControlMesh);
	ControlMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	ControlMesh->CanCharacterStepUpOn = ECB_No;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
	if (Cube.Succeeded()) ControlMesh->SetStaticMesh(Cube.Object);
	ControlLever = CreateDefaultSubobject<UStageControlLeverComponent>(TEXT("ControlLever"));
	ControlLever->SetupAttachment(ControlMesh);
}

bool AStageTurntableControl::CanInteract_Implementation(const FInteractionContext& Context) const
{
	return IsValid(Context.Interactor) && IsValid(Turntable) && Turntable->CanRotate();
}

EInteractionResult AStageTurntableControl::OnInteraction_Implementation(const FInteractionContext& Context)
{
	if (Context.InteractionPhase != EInteractionPhase::Start || Context.bCancelled || !CanInteract_Implementation(Context)) return EInteractionResult::Rejected;
	if (!Turntable->RotateNext()) return EInteractionResult::Rejected;
	ControlLever->PullToward(Context.Interactor->GetActorLocation());
	if (auto* Interaction = Context.Interactor->FindComponentByClass<UInteractionComponent>())
		Interaction->PlayHandMotion(EInteractionHandMotion::Pull, ControlLever->GetGripLocation(), ControlLever);
	return EInteractionResult::Completed;
}

FName AStageTurntableControl::GetInteractionAnimationId_Implementation() const
{
	static const FName Id(TEXT("StageTurntableLever"));
	return Id;
}
