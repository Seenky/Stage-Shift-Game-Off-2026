// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/StageMemberCharacter.h"

#include "StatusesComponent.h"
#include "Components/Dialogue/DialogueBubbleWidget.h"
#include "Components/Dialogue/DialogueComponent.h"


AStageMemberCharacter::AStageMemberCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	StageMemberComponent = CreateDefaultSubobject<UStageMemberComponent>("Stage Member Component");
	StatusesComponent = CreateDefaultSubobject<UStatusesComponent>("Statuses Component");
	DialogueComponent = CreateDefaultSubobject<UDialogueComponent>(TEXT("DialogueComponent"));
	DialogueWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DialogueWidgetComponent"));
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));

	PhysicsManipulationComponent = CreateDefaultSubobject<UPhysicsManipulationComponent>(TEXT("PhysicsManipulation"));
	CarryPoint = CreateDefaultSubobject<USceneComponent>("Carry Point");
	CarryPoint->SetupAttachment(GetRootComponent());
	CarryPoint->SetRelativeLocation(FVector(75.0, 0.0, 25.0));
	PhysicsManipulationComponent->SetCarryPoint(CarryPoint);

	DialogueWidgetComponent->SetupAttachment(GetMesh());
	DialogueWidgetComponent->SetRelativeLocation(FVector(0.0, 0.0, 250.0));
	DialogueWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	DialogueWidgetComponent->SetDrawAtDesiredSize(true);
	DialogueWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FClassFinder<UDialogueBubbleWidget> DialogueWidgetClass(TEXT("/Game/DEV/UI/Dialogue/W_DialogueBubble"));

	if (DialogueWidgetClass.Succeeded())
	{
		DialogueWidgetComponent->SetWidgetClass(DialogueWidgetClass.Class);
	}

	DialogueComponent->SetWidgetComponentProperty(GET_MEMBER_NAME_CHECKED(AStageMemberCharacter, DialogueWidgetComponent));
}

void AStageMemberCharacter::BeginPlay()
{
	Super::BeginPlay();

	PhysicsManipulationComponent->SetCarryPoint(CarryPoint);
	if (!CarrySocketName.IsNone() && GetMesh()->DoesSocketExist(CarrySocketName))
	{
		CarryPoint->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, CarrySocketName);
	}
}


