#pragma once

#include "Components/Interaction/InteractionInterface.h"
#include "Components/Dialogue/StageDialogueTypes.h"
#include "GameFramework/Actor.h"
#include "StageSoundControl.generated.h"

class APlayerController;
class USoundBase;
class UStageControlMeshComponent;
class UStaticMeshComponent;
class UUserWidget;
class UWidgetComponent;

UENUM(BlueprintType)
enum class EStageSoundResult : uint8
{
	Ready, Early, OnTime, Late, Missed, Cancelled, WrongSound
};

USTRUCT(BlueprintType)
struct FStageSoundOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Stage|Sound") FName SoundId;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound") FText DisplayName;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound") TObjectPtr<USoundBase> Sound;
};

USTRUCT(BlueprintType)
struct FStageSoundTiming
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.0", Units = "s")) float ExpectedDelay = 3.f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.0", Units = "s")) float EarlyTolerance = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.0", Units = "s")) float LateTolerance = 0.7f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.0", Units = "s")) float LateGrace = 2.f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "1")) int32 MaxAttempts = 2;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.1", Units = "s")) float RetryDelay = 0.65f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound") bool bRetryLate = false;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound") bool bUntimed = false;

	float GetDuration() const;
};

USTRUCT(BlueprintType)
struct FStageSoundWindow
{
	GENERATED_BODY()
	uint32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") FName CueId;
	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") FName SoundId;
	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") TObjectPtr<AActor> Target;
	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") float OpenedAt = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") float EarliestAt = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") float LatestAt = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") float ExpiresAt = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") int32 Attempt = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Stage|Sound") bool bActive = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStageSoundResultEvent, const FStageSoundWindow&, Window, EStageSoundResult, Result);

UCLASS(Blueprintable)
class STAGESHIFT_API AStageSoundControl : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStageSoundControl();
	virtual const UMeshComponent* GetInteractionMesh() const override;
	virtual bool CanInteract_Implementation(const FInteractionContext& Context) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& Context) override;
	virtual FName GetInteractionAnimationId_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "Stage|Sound")
	bool OpenWindow();
	bool OpenSoundWindow(FName SoundId, AActor* Target, const FStageSoundTiming& Timing);
	UFUNCTION(BlueprintCallable, Category = "Stage|Sound")
	void CancelWindow();
	UFUNCTION(BlueprintPure, Category = "Stage|Sound")
	FStageSoundWindow GetSoundWindow() const { return Window; }
	float GetWindowDuration() const;
	bool HasSound(FName SoundId) const;
	void SetSelectorFocused(bool bFocused);
	void CycleSound(int32 Direction);
	bool CanCycleSound() const { return SoundOptions.Num() > 1; }
	UFUNCTION(BlueprintPure, Category = "Stage|Sound")
	FName GetSelectedSoundId() const;
	UFUNCTION(BlueprintPure, Category = "Stage|Sound")
	EStageSoundResult GetLastCueResult() const { return LastResult; }
	UFUNCTION(BlueprintCallable, Category = "Stage|Sound")
	bool PlaySignal();

	UPROPERTY(BlueprintAssignable, Category = "Stage|Sound") FStageSoundResultEvent OnCueResult;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> ControlMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStageControlMeshComponent> ControlVisual;
	UPROPERTY(EditAnywhere, Category = "Interaction|Animation")
	FName ButtonSocket = TEXT("InteractionGrip");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UWidgetComponent> SoundSelector;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (TitleProperty = "SoundId"))
	TArray<FStageSoundOption> SoundOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Sound") FName CueId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Sound") TObjectPtr<USoundBase> Sound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Sound") TSoftObjectPtr<AActor> SoundTarget;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound") TMap<EStageSoundResult, FDialogueLine> TargetResponseLines;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.0", Units = "s")) float ExpectedDelay = 3.f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.0", Units = "s")) float EarlyTolerance = 0.6f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.0", Units = "s")) float LateTolerance = 0.7f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.0", Units = "s")) float LateGrace = 2.f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "1")) int32 MaxAttempts = 2;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound", meta = (ClampMin = "0.1", Units = "s")) float RetryDelay = 0.65f;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound") bool bRetryLate = false;
	UPROPERTY(EditAnywhere, Category = "Stage|Sound") bool bCanArmFromConsole = false;

private:
	bool StartWindow(FName InCueId, FName SoundId, AActor* Target, const FStageSoundTiming& Timing);
	bool PreviewSelectedSound(APlayerController& Player);
	FStageSoundTiming GetLegacyTiming() const;
	void RefreshSelector();
	void SetTiming(float OpenedAt);
	void ExpireWindow();
	void CloseWindow();
	void BroadcastResult(EStageSoundResult Result);
	UPROPERTY(Transient) FStageSoundWindow Window;
	FStageSoundTiming WindowTiming;
	uint32 LastWindowId = 0;
	FTimerHandle WindowTimer;
	int32 SelectedSoundIndex = 0;
	float ButtonPressTime = 0.0f;
	float NextAttemptAt = 0;
	float NextPreviewAt = 0;
	uint8 ReportedResults = 0;
	EStageSoundResult LastResult = EStageSoundResult::Cancelled;
};
