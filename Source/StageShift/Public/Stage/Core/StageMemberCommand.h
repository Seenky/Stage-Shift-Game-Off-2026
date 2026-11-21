#pragma once

#include "CoreMinimal.h"
#include "Stage/Core/Data/StageData.h"
#include "UObject/Object.h"
#include "StageMemberCommand.generated.h"

class UStageMemberCommand;

UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EStageCommandResource : uint8
{
	None = 0,
	Movement = 1,
	Rotation = 2,
	Speech = 4
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStageCommandFinished, UStageMemberCommand*, Command, EStageCommandResult, Result);

// A task and its progress. Concrete engine work belongs to UStageAction.
UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)

class STAGESHIFT_API UStageMemberCommand : public UObject
{
	GENERATED_BODY()

public:
	// Property path -> stable graph parameter ID. Names may change without breaking bindings.
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Graph Parameters")
	TMap<FName, FGuid> GraphParameterBindings;
	// Blueprint command classes declare resources without an editor-specific implementation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Command", meta = (Bitmask, BitmaskEnum = "/Script/StageShift.EStageCommandResource", NoGraphParameter))
	int32 DeclaredResources = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Advanced", meta = (GetOptions = "GetRoleOptions"))
	FName RequiredSuccessRoleId;

	UFUNCTION()
	TArray<FName> GetRoleOptions() const;
	UFUNCTION()
	TArray<FName> GetTargetOptions() const;
	virtual uint8 GetResourceMask() const;
	virtual bool CanRunWhileFallen() const { return false; }
	virtual bool ExpectsStageProp() const { return false; }
	virtual TOptional<TRange<double>> GetPropThrowWindow() const { return {}; }
	virtual void ValidateDefinition(const class UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const;
	virtual void ValidateContext(const FStageExecutionContext& InContext, const FString& Path, TArray<FString>& Errors) const;
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid EditorGuid;
#endif
	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	EStageCatchUpPolicy CatchUpPolicy = EStageCatchUpPolicy::Keep;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	FStageExecutionRequirements ExecutionRequirements;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced", meta = (ClampMin = "0", ToolTip = "Number of retries after the Main Action fails. For Sequence and Parallel, configure the child Command."))
	int32 MaxRetries = 0;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced", meta = (ClampMin = "0.01", Units = "s"))
	float RetryInterval = 0.5f;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	bool bRequireStageLocation = false;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	bool bRequireStageYaw = false;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced", meta = (ClampMin = "0", UIMin = "0"))
	int32 ScoreValue = 0;

	UPROPERTY(AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	FGameplayTag OnSuccessEventTag;

	UPROPERTY(BlueprintAssignable, Category = "Stage|Command")
	FStageCommandFinished OnFinished;

	void PreStart(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Stage|Command")
	void Start(const FStageExecutionContext& InContext);

	UFUNCTION(BlueprintCallable, Category = "Stage|Command")
	void RequestStop(EStageActionStopReason Reason);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	float GetNeededTime() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	bool IsResultAchieved(const FStageExecutionContext& InContext) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Stage|Command")
	UStageAction* CreateMainAction(const FStageExecutionContext& InContext);

	void NotifyMainActionStarted();
	void ApplyMainActionResult(UStageAction* Action, EStageActionResult ActionResult);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Stage|Command")
	FStageEvent BuildSuccessEvent() const;

	virtual void ResetRuntime(const FStageExecutionContext& InContext, UStageMemberCommand* InRoot, float InPlannedStartTime, float InPlannedEndTime);

	virtual bool IsComposite() const { return false; }

	virtual const TArray<TObjectPtr<UStageMemberCommand>>& GetChildCommands() const;
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintPure, Category = "Stage|Command")

	EStageExecutionState GetState() const { return CommandState; }
	UFUNCTION(BlueprintPure, Category = "Stage|Command")

	EStageCommandResult GetResult() const { return CommandResult; }

	UStageMemberCommand* GetRootCommand() const { return RootCommand.Get(); }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")

	float GetPlannedStartTime() const { return PlannedStartTime; }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")

	float GetPlannedEndTime() const { return PlannedEndTime; }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")
	float GetAccumulatedActiveTime() const;

	UFUNCTION(BlueprintPure, Category = "Stage|Command")

	bool IsUsingShortVersion() const { return bUseShortVersion; }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")

	bool HasStartedMainAction() const { return bHasStartedMainAction; }

	UFUNCTION(BlueprintPure, Category = "Stage|Command")

	bool HasSuccessfulMainAction() const { return bHasSuccessfulMainAction; }

protected:
	UPROPERTY()
	FStageExecutionContext Context;

	UFUNCTION(BlueprintCallable, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void Finish(EStageCommandResult InResult);

	// Check before starting or resuming Main; switching remains explicit through UseShortVersion().
	UFUNCTION(BlueprintPure, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	bool ShouldUseShortVersion(float ShortDuration) const;

	UFUNCTION(BlueprintCallable, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void UseShortVersion();

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnPreStart(const FStageExecutionContext& InContext);

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnStart();

	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnStop(EStageActionStopReason Reason);

	UFUNCTION(BlueprintCallable, Category = "Stage|Command", meta = (BlueprintProtected = "true", AutoCreateRefTerm="InTargetId"))
	AActor* ResolveTarget(const FStageExecutionContext& InContext, const FName& InTargetId) const;

	/*
Called every time when main action call finish
(preferred to save progress)
*/
	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Command", meta = (BlueprintProtected = "true"))
	void OnMainActionResult(UStageAction* Action, EStageActionResult ActionResult);

private:
	friend class UStageMemberComponent;

	void SaveAccumulatedActiveTime();

	EStageExecutionState CommandState = EStageExecutionState::NotStarted;
	EStageCommandResult CommandResult = EStageCommandResult::None;

	UPROPERTY(Transient)
	TWeakObjectPtr<UStageMemberCommand> RootCommand;

	float PlannedStartTime = 0.0f;
	float PlannedEndTime = 0.0f;
	// Accumulated time of main action
	float AccumulatedActiveTime = 0.0f;
	// Start time of current main action
	float CurrentStartTime = -1.0f;
	bool bPreStarted = false;
	bool bUseShortVersion = false;
	bool bHasStartedMainAction = false;
	bool bHasSuccessfulMainAction = false;
};
