// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "StageData.h"
#include "StageManagerSubsystem.generated.h"

class AStageManager;
class UStateTreeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnStageMemberStateChanged, 
	const FStageMemberData&, StageMember, 
	const FGameplayTag&, InMemberState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnStageMemberNumberChanged,
	const FStageMemberData&, MemberData,
	const bool&, bIsAdded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnStageStarted,
	const FStageData&, StageData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnStageStopped,
	const FStageData&, StageData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnStageStateChanged,
	const FStageData&, StageData,
	const EStageState&, StageState);

UCLASS()
class STAGESHIFT_API UStageManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	
	/* Delegates */
	
	UPROPERTY(BlueprintAssignable, Category="Stage")
	FOnStageStarted OnStageStarted;
	UPROPERTY(BlueprintAssignable, Category="Stage")
	FOnStageStopped OnStageStopped;
	UPROPERTY(BlueprintAssignable, Category="Stage")
	FOnStageStateChanged OnStageStateChanged;
	UPROPERTY(BlueprintAssignable, Category="Member")
	FOnStageMemberStateChanged OnStageMemberStateChanged;
	UPROPERTY(BlueprintAssignable, Category="Member")
	FOnStageMemberNumberChanged OnStageMemberNumberChanged;
	
	/* Functions */
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Stage", meta=(AutoCreateRefTerm="InStageTag"))
	bool SelectStage(
		UPARAM(meta=(Categories = "Stage.Scene"))	
		const FGameplayTag& InStageTag);
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Stage")
	bool RequestStopCurrentStage();
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Stage")
	bool RequestFinishCurrentStage();
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Stage")
	bool StopCurrentStage();
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Stage")
	EStageState GetCurrentStageState() const { 
		return CurrentStage.IsValid() ? 
		CurrentStage.StageState : EStageState::None; }
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Member", meta=(AutoCreateRefTerm="InMemberTag"))
	bool GetStageMemberData(
		UPARAM(meta=(Categories = "Stage.Member"))
		const FGameplayTag& InMemberTag, 
		FStageMemberData& MemberData);
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Member", meta=(AutoCreateRefTerm="InMemberData"))
	bool SetStageMemberData(const FStageMemberData& InMemberData);
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Member", meta=(AutoCreateRefTerm="InMemberData"))
	bool AddMember(const FStageMemberData& InMemberData);
	
	UFUNCTION(BlueprintCallable, Category="StageManager|Member", meta=(AutoCreateRefTerm="InMemberTag"))
	bool RemoveMember(
		UPARAM(meta=(Categories = "Stage.Member"))
		const FGameplayTag& InMemberTag);
	
private:
	
	TObjectPtr<UStateTreeComponent> GetStateTreeComponentFromManager() const;
	void CheckStageManager();
	bool SetStageStage(const EStageState& InStageState);
	void FinishCurrentStage();
	
	/* Bindings */
	UFUNCTION()
	void HandleStateTreeChangedStatus(EStateTreeRunStatus StateTreeRunStatus);
	
	/* Service */
	
	UPROPERTY()
	TArray<FStageData> AvailableStages;
	FStageData CurrentStage;
	
	UPROPERTY()
	TWeakObjectPtr<AStageManager> StageManager = nullptr;
};
