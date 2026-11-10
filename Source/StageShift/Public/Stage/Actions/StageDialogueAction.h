// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Dialogue/DialogueComponent.h"
#include "Components/Dialogue/StageDialogueTypes.h"
#include "Stage/Core/StageAction.h"
#include "StageDialogueAction.generated.h"


UCLASS()
class STAGESHIFT_API UStageDialogueAction : public UStageAction
{
	GENERATED_BODY()
	
public:
	
	void SetupDialogueAction(
		const FDialogueLine& InLine,
		const bool bInResumeLine,
		const TObjectPtr<UDialogueComponent>& InDialogueComponent);

protected:
	
	virtual void OnStart_Implementation() override;
	virtual void OnStop_Implementation(EStageActionStopMode Mode) override;
	virtual void Cleanup_Implementation() override;
	virtual EStageActionStopMode GetStopMode_Implementation(EStageActionStopReason Reason) const override;

	
private:
	
	UFUNCTION()
	void HandleDialogueEnded(FGuid PlaybackId, EDialogueEndReason Reason);
	
	UPROPERTY()
	FDialogueLine CurrentLine;
	UPROPERTY()
	TWeakObjectPtr<UDialogueComponent> DialogueComponent;
	FGuid CurrentPlaybackId;
	bool bResumeLine = false;
};

