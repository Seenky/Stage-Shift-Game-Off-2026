// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Actor.h"
#include "MainCameraActor.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS(HideCategories = (Actor, Rendering, Replication, Collision, Input, LOD, Cooking, AutoPlayerActivation, WorldPartition, LevelInstance, HLOD, Navigation, AssetUserData, Activation, Tags, Physics))
class STAGESHIFT_API AMainCameraActor : public ACameraActor
{
	GENERATED_BODY()

public:
	AMainCameraActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArmComponent;
};
