#pragma once

#include "Stage/Core/StageAction.h"
#include "Stage/Environment/StageSoundControl.h"
#include "StageSoundCueAction.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;

UCLASS(NotBlueprintable)
class STAGESHIFT_API UStageSoundCueAction : public UStageAction
{
	GENERATED_BODY()

public:
	void Setup(AStageSoundControl* InControl, UAnimSequenceBase* InWindupAnimation, FName InAnimationSlot, FName InCueAnchorId, bool bInRequireOnTime);
	void SetupSound(FName InSoundId, AActor* InSoundTarget, const FStageSoundTiming& InTiming);

protected:
	virtual void OnStart_Implementation() override;
	virtual void Cleanup_Implementation() override;

private:
	void PlayWindup();
	void FinishWindow();
	UFUNCTION()
	void HandleSoundResult(const FStageSoundWindow& Window, EStageSoundResult Result);

	TWeakObjectPtr<AStageSoundControl> Control;
	UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> WindupAnimation;
	TWeakObjectPtr<UAnimInstance> AnimInstance;
	TWeakObjectPtr<UAnimMontage> WindupMontage;
	FName AnimationSlot;
	FName CueAnchorId;
	FName SoundId;
	TWeakObjectPtr<AActor> SoundTarget;
	FStageSoundTiming Timing;
	FName CueId;
	uint32 WindowId = 0;
	float WindupDuration = 0.0f;
	bool bRetainCueFeedback = false;
	bool bRequireOnTime = false;
	FTimerHandle WindupTimer;
	FTimerHandle EndTimer;
};
