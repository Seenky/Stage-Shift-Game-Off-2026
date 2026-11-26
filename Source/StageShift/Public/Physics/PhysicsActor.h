// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Interaction/InteractionOutlineStencil.h"
#include "PhysicsTypes.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "StageShift/Public/Components/Interaction/InteractionInterface.h"
#include "PhysicsActor.generated.h"

class UPhysicsManipulationComponent;
class AStagePhysicsActor;
class AStageCurtainRail;

UENUM(BlueprintType)
enum class EStagePropTransferResult : uint8
{
	HandedOff, Thrown, Caught, Missed, Recovered
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FStagePropTransferEvent, AStagePhysicsActor*, Prop, AActor*, From, AActor*, To, EStagePropTransferResult, Result, float, CatchWindowSeconds);

UCLASS()

class STAGESHIFT_API AStagePhysicsActor : public AStaticMeshActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStagePhysicsActor();

	//** IInteractionInterface
	virtual bool CanInteract_Implementation(const FInteractionContext& InteractionContext) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& InteractionContext) override;
	virtual FName GetInteractionAnimationId_Implementation() const override;
	//** IInteractionInterface

	UFUNCTION(BlueprintPure)

	EPhysicsInteractionType GetInteractionType() const { return InteractionType; }

	UFUNCTION(BlueprintCallable, Category = "Stage|Props")
	bool TryHandOff(AActor* From, AActor* To);
	bool CanHandOff(const AActor* From, const AActor* To) const;
	float GetHandoffRange() const { return HandoffRange; }
	void WatchThrownProp(AActor* Thrower);
	void IgnoreCatchFor(AActor* Actor) { IgnoredCatchActor = Actor; }
	void NotifyPropCarried(AActor* Carrier);
	void SetCurtainRail(AStageCurtainRail* Rail);
	virtual const UMeshComponent* GetInteractionMesh() const override;
	bool IsCurtainLever() const { return CurtainRail.IsValid(); }
	bool IsBeingManipulated() const;
	bool IsBeingDragged() const;
	void SetPickupCueHighlighted(UObject& Source, bool bHighlighted);

	UPROPERTY(BlueprintAssignable, Category = "Stage|Props")
	FStagePropTransferEvent OnPropTransfer;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	EPhysicsInteractionType InteractionType = EPhysicsInteractionType::None;
	UPROPERTY(EditAnywhere, Category = "Stage|Props", meta = (ClampMin = "1.0", Units = "cm"))
	float HandoffRange = 185.f;
	UPROPERTY(EditAnywhere, Category = "Stage|Props", meta = (ClampMin = "1.0", Units = "cm"))
	float CatchRadius = 65.f;
	UPROPERTY(EditAnywhere, Category = "Stage|Props", meta = (ClampMin = "0.05", Units = "s"))
	float CatchWindow = 1.25f;

	UPROPERTY(EditAnywhere, Category = "Audio")
	TObjectPtr<class USoundBase> ImpactSound;

private:
	friend class UInteractionOutlineComponent;
	friend class UPhysicsManipulationComponent;

	void RefreshPickupOutline();
	bool SetPickupOutlineFocused(bool bFocused);
	int32 GetPickupOutlineMask(bool bFocused) const;
	void ConfigureCharacterContact();
	void CheckCatch();
	void IgnoreThrowerDuringRelease(const AActor& Source);
	void RestoreThrowerCollision();
	bool HasClearReach(FVector From, FVector To, const AActor* Source, const AActor* Receiver) const;
	UFUNCTION()
	void HandleImpactSound(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	    FVector NormalImpulse, const FHitResult& Hit);

	FTimerHandle CatchTimer;
	FTimerHandle ThrowerCollisionTimer;
	TWeakObjectPtr<AActor> Thrower;
	TWeakObjectPtr<AActor> IgnoredCatchActor;
	FVector PreviousFlightPosition;
	double CatchDeadline = 0;
	double LastImpactSoundTime = -1.0;
	bool bMissedThrow = false;
	FInteractionOutlineStencil PickupOutline;
	TSet<TWeakObjectPtr<UObject>> PickupCueHighlightSources;
	bool bPickupOutlineFocused = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPhysicsManipulationComponent> ActiveManipulator;
	TWeakObjectPtr<AStageCurtainRail> CurtainRail;
};
