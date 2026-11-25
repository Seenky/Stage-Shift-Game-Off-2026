#pragma once

#include "Components/Interaction/InteractionInterface.h"
#include "GameFramework/Actor.h"
#include "StageBackstageRope.generated.h"

class AStageShiftCharacter;
class UBoxComponent;
class UStaticMeshComponent;
class UTexture2D;
class UWidgetComponent;

UCLASS()
class STAGESHIFT_API AStageBackstageRope : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStageBackstageRope();
	virtual bool CanInteract_Implementation(const FInteractionContext& Context) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& Context) override;
	virtual const UMeshComponent* GetInteractionMesh() const override;
	virtual bool CanInteractWhileCarrying() const override { return true; }
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	AStageBackstageRope* GetPartner() const;
	bool IsAvailable() const;
	bool FindLanding(const AStageShiftCharacter& Player, FVector& Location) const;
	bool DropTraveller(AStageShiftCharacter& Player, const AStageBackstageRope& Destination);
	void SetGripOffset(const FVector& Offset);
	void SetTravellerHidden(bool bHide);
	void UpdateSignal();
	void FinishTrip(bool bDropped);

	UFUNCTION()
	void OnApproach(UPrimitiveComponent* Component, AActor* Actor, UPrimitiveComponent* Other, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);
	UFUNCTION()
	void OnLeave(UPrimitiveComponent* Component, AActor* Actor, UPrimitiveComponent* Other, int32 BodyIndex);

	UPROPERTY(EditInstanceOnly, Category = "Rope")
	TObjectPtr<AStageBackstageRope> Partner;
	UPROPERTY(VisibleAnywhere, Category = "Rope")
	TObjectPtr<UBoxComponent> Approach;
	UPROPERTY(VisibleAnywhere, Category = "Rope")
	TObjectPtr<UBoxComponent> UseTarget;
	UPROPERTY(VisibleAnywhere, Category = "Rope")
	TObjectPtr<UStaticMeshComponent> Grip;
	UPROPERTY(VisibleAnywhere, Category = "Rope")
	TArray<TObjectPtr<UStaticMeshComponent>> RopeSections;
	UPROPERTY(VisibleAnywhere, Category = "Rope")
	TObjectPtr<UWidgetComponent> Signal;
	UPROPERTY(EditAnywhere, Category = "Rope")
	TObjectPtr<UTexture2D> Icon;

	TWeakObjectPtr<AStageShiftCharacter> Traveller;
	TWeakObjectPtr<AActor> HeldProp;
	FVector Origin = FVector::ZeroVector;
	FVector PullDirection = FVector::ZeroVector;
	double StartedAt = 0.0;
	bool bSavedDollPhysics = false;
	bool bPlayerWasHidden = false;
	bool bPropWasHidden = false;
	bool bTravellerHidden = false;
};
