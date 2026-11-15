// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/MainCameraActor.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"


AMainCameraActor::AMainCameraActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>("SpringArm");

	SpringArmComponent->SetupAttachment(GetRootComponent());

	SpringArmComponent->bDoCollisionTest = false;
	SpringArmComponent->bUsePawnControlRotation = false;
	SpringArmComponent->bInheritYaw = true;
	SpringArmComponent->bInheritPitch = true;
	SpringArmComponent->bInheritRoll = false;

	GetCameraComponent()->SetupAttachment(SpringArmComponent);
}

void AMainCameraActor::BeginPlay()
{
	GetWorld()->RegisterAutoActivateCamera(this, 0);

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC)
	{
		PC->SetViewTarget(this);
	}
}


