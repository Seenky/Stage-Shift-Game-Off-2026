#pragma once

#include "CoreMinimal.h"
#include "Components/Interaction/InteractionInterface.h"
#include "GameFramework/Actor.h"
#include "StageTurntableControl.generated.h"

class AStageTurntable;
class UStageControlLeverComponent;
class UStaticMeshComponent;

/** A stationary control using the same interaction path as other stage props. */
UCLASS(Blueprintable)
class STAGESHIFT_API AStageTurntableControl : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStageTurntableControl();
	virtual bool CanInteract_Implementation(const FInteractionContext& Context) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& Context) override;
	virtual FName GetInteractionAnimationId_Implementation() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turntable")
	TObjectPtr<UStaticMeshComponent> ControlMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turntable")
	TObjectPtr<UStageControlLeverComponent> ControlLever;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Turntable")
	TObjectPtr<AStageTurntable> Turntable;
};
