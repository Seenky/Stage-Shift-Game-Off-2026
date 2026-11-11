// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Dialogue/StageDialogueTypes.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StageDialogueCommand.generated.h"

UCLASS()
class STAGESHIFT_API UStageDialogueCommand : public UStageMemberCommand
{
	GENERATED_BODY()
	
public:
	
	virtual float GetNeededTime_Implementation() const override;
	virtual UStageAction* CreateMainAction_Implementation(const FStageExecutionContext& InContext) override;
	virtual void OnMainActionResult_Implementation(UStageAction* Action, EStageActionResult ActionResult) override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;
private:
	
	UPROPERTY(EditAnywhere)
	FDialogueLine CurrentLine;
	
	bool bIsNeedResumeLine;
	bool bDialogueCompleted;
};
