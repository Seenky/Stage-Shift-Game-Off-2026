// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsTypes.generated.h"

UENUM(BlueprintType)
enum class EPhysicsInteractionType : uint8
{
	None,
	Carry, 
	Drag   
};
