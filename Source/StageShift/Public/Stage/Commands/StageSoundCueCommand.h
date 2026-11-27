#pragma once

#include "Stage/Core/StageMemberCommand.h"
#include "Stage/Environment/StageSoundControl.h"
#include "StageSoundCueCommand.generated.h"

class UAnimSequenceBase;

UCLASS(meta = (DisplayName = "Sound Cue", StageCategory = "Interaction"))
class STAGESHIFT_API UStageSoundCueCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual void ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const override;
	virtual int32 GetEarnedScore() const override { return bPlayedOnTime ? Super::GetEarnedScore() : 0; }

protected:
	virtual void OnPreStart_Implementation(const FStageExecutionContext& InContext) override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
	virtual float GetNeededTime_Implementation() const override { return NeededTime; }
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual void OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult) override;

private:
	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (GetOptions = "GetTargetOptions"))
	FName SoundControlId;

	UPROPERTY(EditAnywhere, Category = "Sound", meta = (ToolTip = "The requested entry on the sound console. Empty uses the console's single-sound settings."))
	FName SoundId;

	UPROPERTY(EditAnywhere, Category = "Sound", meta = (GetOptions = "GetTargetOptions"))
	FName SoundTargetId;

	UPROPERTY(EditAnywhere, Category = "Sound")
	FStageSoundTiming Timing;

	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (GetOptions = "GetTargetOptions"))
	FName RequiredPropId;

	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (GetOptions = "GetTargetOptions"))
	FName CueAnchorId;

	UPROPERTY(EditAnywhere, Category = "Scenario", meta = (ToolTip = "Take On Failed if the player does not make an on-time sound."))
	bool bRequireOnTime = false;

	UPROPERTY(EditAnywhere, Category = "Scenario")
	TObjectPtr<UAnimSequenceBase> WindupAnimation;

	UPROPERTY(EditAnywhere, Category = "Scenario")
	FName AnimationSlot = TEXT("RightHand");

	float NeededTime = 0.0f;
	bool bPlayedOnTime = false;
};
