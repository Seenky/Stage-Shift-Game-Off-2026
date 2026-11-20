#pragma once

#include "Stage/Core/StageAction.h"
#include "StageImpactAction.generated.h"

class AStagePhysicsActor;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EStageImpactMode : uint8
{
	Strike,
	Throw,
	Collapse
};

UCLASS(NotBlueprintable)
class STAGESHIFT_API UStageImpactAction : public UStageAction
{
	GENERATED_BODY()

public:
	void Setup(EStageImpactMode InMode, AActor* InTarget, AStagePhysicsActor* InProp, float InDelay,
	    float InStageTimeRequired, UAnimSequenceBase* InWindup, UAnimMontage* InFall);

protected:
	virtual void OnStart_Implementation() override;
	virtual EStageActionStopMode GetStopMode_Implementation(EStageActionStopReason Reason) const override;
	virtual void Cleanup_Implementation() override;

private:
	void PerformImpact();
	void FinishHit();
	void Fail(const TCHAR* Reason);
	UFUNCTION() void HandlePropHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	    FVector NormalImpulse, const FHitResult& Hit);

	EStageImpactMode Mode = EStageImpactMode::Collapse;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AStagePhysicsActor> Prop;
	TWeakObjectPtr<UPrimitiveComponent> FlightBody;
	TWeakObjectPtr<UAnimInstance> AnimInstance;
	TWeakObjectPtr<UAnimMontage> PlayingWindup;
	UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> Windup;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> Fall;
	float Delay = 0.0f;
	float StageTimeRequired = 0.0f;
	bool bRestoreHitNotifications = false;
	bool bRestoreCCD = false;
	FTimerHandle ImpactTimer;
	FTimerHandle FlightTimer;
};
