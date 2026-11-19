// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageAction.h"
#include "WaitForItemAction.generated.h"

class UPhysicsManipulationComponent;

UCLASS()
class STAGESHIFT_API UWaitForItemAction : public UStageAction
{
	GENERATED_BODY()

public:

	void SetupWaitForItem(
		UPhysicsManipulationComponent* InPhysicsManipulationComponent,
		AActor* InItem);

protected:

	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;
private:

	UFUNCTION()
	void OnPickupItem(AActor* InItem);

	TWeakObjectPtr<UPhysicsManipulationComponent> PhysicsManipulationComponent = nullptr;
	TWeakObjectPtr<AActor> NeededItem = nullptr;
};
