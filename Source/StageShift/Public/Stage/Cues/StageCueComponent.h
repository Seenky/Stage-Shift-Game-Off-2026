#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Stage/Cues/StageCueTypes.h"
#include "Stage/Core/Data/StageData.h"
#include "StageCueComponent.generated.h"

class AStageManager;
class AStageCueWorldMarker;
class UStageDataAsset;
class UStageCueHUDWidget;
class UStageCueSignalWidget;
class UWidgetComponent;
class UTexture2D;
struct FStageSoundWindow;
enum class EStageSoundResult : uint8;

DECLARE_MULTICAST_DELEGATE(FStageCuesChanged);

// Scenario-owned state. Widgets and world markers only present this state.
UCLASS(ClassGroup = (Stage), meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UStageCueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStageCueComponent();
	UPROPERTY(EditAnywhere, Category = "Presentation|Signals") bool bUseWorldSignals = true;
	UPROPERTY(EditAnywhere, Category = "Presentation|Signals", meta = (ToolTip = "Shared location for signals without a bound actor."))
	TSoftObjectPtr<AActor> SignalAnchor;
	UPROPERTY(EditAnywhere, Category = "Presentation|Signals") FVector SignalOffset = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, Category = "Presentation|Signals") TMap<EStageCueKind, TObjectPtr<UTexture2D>> SignalIcons;
	UPROPERTY(EditAnywhere, Category = "Presentation|Signals") TSoftClassPtr<UStageCueSignalWidget> SignalWidgetClass;
	UPROPERTY(EditAnywhere, Category = "Presentation|Signals", meta = (ClampMin = "0.1", Units = "s")) float UntimedSignalDuration = 3.0f;
	UPROPERTY(EditAnywhere, Category = "Presentation") TSubclassOf<UStageCueHUDWidget> WidgetClass;
	UPROPERTY(EditAnywhere, Category = "Presentation") TSubclassOf<AStageCueWorldMarker> MarkerClass;
	UPROPERTY(EditAnywhere, Category = "Presentation", meta = (ClampMin = "1", ClampMax = "5")) int32 MaxCards = 3;
	UPROPERTY(EditAnywhere, Category = "Presentation", meta = (ClampMin = "0", Units = "s")) float CompletedDisplayTime = 1.5f;

	UFUNCTION(BlueprintCallable, Category = "Stage|Cues") bool ShowCue(const FStageCueDefinition& Definition);
	UFUNCTION(BlueprintCallable, Category = "Stage|Cues") void ActivateCue(FName CueId);
	UFUNCTION(BlueprintCallable, Category = "Stage|Cues") void ResolveCue(FName CueId);
	UFUNCTION(BlueprintCallable, Category = "Stage|Cues") void ReportProblem(FName CueId, const FText& Details);
	UFUNCTION(BlueprintCallable, Category = "Stage|Cues") void DismissCue(FName CueId);
	// Gameplay systems can reschedule their own windows without resetting the cue.
	UFUNCTION(BlueprintCallable, Category = "Stage|Cues") void SetCueTimeRemaining(FName CueId, float Seconds);
	UFUNCTION(BlueprintCallable, Category = "Stage|Cues") void ShowActorProblem(FName CueId, const FText& Title, const FText& Details, AActor* Actor, float TimeLimit = 0.0f);
	void ShowActorSignal(FName CueId, EStageCueKind Kind, AActor* Actor);
	UFUNCTION(BlueprintPure, Category = "Stage|Cues") TArray<FStageCueView> GetVisibleCues() const;
	UFUNCTION(BlueprintPure, Category = "Stage|Cues") TArray<AStageCueWorldMarker*> GetVisibleMarkers() const;
	FStageCuesChanged OnCuesChanged;

	void StartStage(UStageDataAsset* Data);
	void NotifyStepStarted(FName StepId);
	void ResetCues();
	void SetSoundWindow(FName CueId, const FStageSoundWindow& Window);
	void ReportSoundResult(FName CueId, EStageSoundResult Result, bool bWindowActive);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void Refresh();
	void UpdatePresentation();
	void UpdateWorldSignals();
	void ClearWorldSignals();
	TArray<int32> GetVisibleIndices(bool bSignalsOnly = false) const;
	AActor* ResolveTarget(const FStageCueTarget& Target, const FStageCueRuntime& Cue) const;
	UFUNCTION() void HandleStageEvent(FStageEvent Event);
	UPROPERTY(Transient) TObjectPtr<UStageDataAsset> DefinitionSource;
	UPROPERTY(Transient) TArray<FStageCueRuntime> Cues;
	UPROPERTY(Transient) TArray<TObjectPtr<UStageCueHUDWidget>> Widgets;
	UPROPERTY(Transient) TMap<FString, TObjectPtr<AStageCueWorldMarker>> Markers;
	UPROPERTY(Transient) TMap<EStageCueKind, TObjectPtr<UWidgetComponent>> WorldSignals;
	TWeakObjectPtr<AStageManager> Manager;
	FTimerHandle RefreshTimer;
	int32 NextNumber = 1;
	bool bEndingPlay = false;
};
