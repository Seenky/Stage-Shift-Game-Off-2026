#pragma once

#include "Stage/Core/StageAction.h"
#include "StageImpactAction.generated.h"

class AStagePhysicsActor;
class AStageSoundControl;
class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class UPrimitiveComponent;
struct FStageSoundWindow;
enum class EStageSoundResult : uint8;

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
	    float InStageTimeRequired, UAnimSequenceBase* InWindup, UAnimSequenceBase* InThrowAnimation, UAnimMontage* InFall,
	    AStageSoundControl* InSoundControl);

protected:
	virtual void OnStart_Implementation() override;
	virtual EStageActionStopMode GetStopMode_Implementation(EStageActionStopReason Reason) const override;
	virtual void Cleanup_Implementation() override;

private:
	void PerformImpact();
	void FinishHit();
	void Fail(const TCHAR* Reason);
	UFUNCTION() void HandleSoundResult(const FStageSoundWindow& Window, EStageSoundResult Result);
	UFUNCTION() void HandlePropHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	    FVector NormalImpulse, const FHitResult& Hit);

	EStageImpactMode Mode = EStageImpactMode::Collapse;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AStagePhysicsActor> Prop;
	TWeakObjectPtr<AStageSoundControl> SoundControl;
	TWeakObjectPtr<UPrimitiveComponent> FlightBody;
	TWeakObjectPtr<UAnimInstance> AnimInstance;
	TWeakObjectPtr<UAnimMontage> PlayingAnimation;
	UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> Windup;
	UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> ThrowAnimation;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> Fall;
	float Delay = 0.0f;
	float StageTimeRequired = 0.0f;
	uint32 SoundWindowId = 0;
	bool bThrowStarted = false;
	bool bRestoreHitNotifications = false;
	bool bRestoreCCD = false;
	FTimerHandle ImpactTimer;
	FTimerHandle FlightTimer;
};
