// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/StageMemberCommand.h"
#include "WaitForItemCommand.generated.h"


UCLASS()
class STAGESHIFT_API UWaitForItemCommand : public UStageMemberCommand
{
	GENERATED_BODY()

public:

	virtual void ValidateDefinition(const class UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
	virtual bool IsResultAchieved_Implementation(const FStageExecutionContext& InContext) const override;

private:

	/*
	 * ItemId from target info
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess, GetOptions = "GetTargetOptions"))
	FName ItemId = NAME_None;
};
