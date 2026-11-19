#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StageCueTypes.generated.h"

class UStageCondition;

UENUM(BlueprintType)
enum class EStageCueState : uint8
{
	Upcoming,
	Active,
	Problem,
	Completed
};

UENUM(BlueprintType)
enum class EStageCueMarkerType : uint8
{
	Object,
	Zone,
	Event,
	Recipient
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FStageCueTarget
{
	GENERATED_BODY()

	// A level-local target or role binding; never a scene-specific Gameplay Tag.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker") FName BindingId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker") EStageCueMarkerType Type = EStageCueMarkerType::Object;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker") FVector Offset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Marker", meta = (ClampMin = "10", Units = "cm", EditCondition = "Type == EStageCueMarkerType::Zone"))
	float Radius = 80.0f;
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FStageCueDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue") FName CueId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue", meta = (MultiLine)) FText Details;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue", meta = (MultiLine)) FText OverdueDetails;
	// These triggers follow actual scenario execution, including branches and delays.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (GetOptions = "GetStepOptions")) FName ShowAtStepId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (GetOptions = "GetStepOptions")) FName ActivateAtStepId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (GetOptions = "GetStepOptions")) FName HideAtStepId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario") FGameplayTag TriggerEvent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (GetOptions = "GetRoleOptions")) FName SubjectRoleId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cue") bool bIsProblem = false;
	// Zero means untimed. An overdue cue stays repairable until resolved or dismissed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0", Units = "s")) float TimeLimit = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Completion") TObjectPtr<UStageCondition> CompletionCondition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers", meta = (TitleProperty = "BindingId")) TArray<FStageCueTarget> Targets;
};

USTRUCT(BlueprintType)
struct STAGESHIFT_API FStageCueView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FName CueId;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Details;
	UPROPERTY(BlueprintReadOnly) EStageCueState State = EStageCueState::Active;
	UPROPERTY(BlueprintReadOnly) int32 Number = 0;
	UPROPERTY(BlueprintReadOnly) bool bHasDeadline = false;
	UPROPERTY(BlueprintReadOnly) bool bOverdue = false;
	UPROPERTY(BlueprintReadOnly) float SecondsRemaining = 0.0f;
	UPROPERTY(BlueprintReadOnly) float TimeFraction = 1.0f;
};

USTRUCT()
struct FStageCueRuntime
{
	GENERATED_BODY()

	UPROPERTY() FStageCueDefinition Definition;
	UPROPERTY() FStageCueView View;
	UPROPERTY() TWeakObjectPtr<AActor> ActorOverride;
	double Deadline = 0.0;
	double CompletedAt = 0.0;
	float TimerDuration = 0.0f;
};
