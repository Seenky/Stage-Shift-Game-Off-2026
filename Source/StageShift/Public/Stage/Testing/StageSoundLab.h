#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageSoundLab.generated.h"

class AStagePhysicsActor;
class AStageShiftCharacter;
class SWidget;
class UAudioComponent;
class UInputAction;
class USoundBase;

UENUM()
enum class EStageSoundLabAction : uint8
{
	Walk, Land, Fall, Stumble, Fabric, Grab, Release, Throw, Catch, Place, Impact, Paper, Slide,
	Cue, Spotlight, RotationStart, RotationLoop, RotationStop, CurtainOpen, CurtainClose,
	UIHover, UIPress, UIConfirm, UIBack, UIUrgent, Applause, Music
};

USTRUCT()
struct FStageSoundLabCase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere) FString Label;
	UPROPERTY(EditAnywhere) FString Hint;
	UPROPERTY(EditAnywhere) EStageSoundLabAction Action = EStageSoundLabAction::Walk;
	UPROPERTY(EditAnywhere) TObjectPtr<AActor> Target;
	UPROPERTY(EditAnywhere) TArray<TObjectPtr<USoundBase>> Takes;
	UPROPERTY(EditAnywhere) TObjectPtr<USoundBase> FallImpactSound;
};

/** Playable audition controls for LVL_SoundLab. All sound changes are per-instance. */
UCLASS(NotBlueprintable)
class STAGESHIFT_API AStageSoundLab : public AActor
{
	GENERATED_BODY()

public:
	AStageSoundLab();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Sound Lab") TArray<FStageSoundLabCase> Cases;
	UPROPERTY(EditAnywhere, Category = "Sound Lab") TObjectPtr<AStagePhysicsActor> Parcel;
	UPROPERTY(EditAnywhere, Category = "Sound Lab") TObjectPtr<AActor> Receiver;
	UPROPERTY(EditAnywhere, Category = "Sound Lab") TObjectPtr<UInputAction> InteractionAction;

	UFUNCTION(BlueprintCallable, Category = "Sound Lab") void SelectCase(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "Sound Lab") void SelectTake(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "Sound Lab") void RunCurrent();
	UFUNCTION(BlueprintCallable, Category = "Sound Lab") void ResetFixtures();

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void NextCase() { SelectCase(1); }
	void PreviousCase() { SelectCase(-1); }
	void NextTake() { SelectTake(1); }
	void PreviousTake() { SelectTake(-1); }
	void Setup(APlayerController& Controller);
	void BindTake(const FStageSoundLabCase& Case, int32 Take) const;
	void PrepareInteraction(AStageShiftCharacter& Pawn, EStageSoundLabAction Action);
	void Restore(AStagePhysicsActor& Prop) const;
	void MoveFixture(AActor& Target, const FVector& Destination, float Seconds, USoundBase* ArrivalSound = nullptr);
	void StopMusic();
	FText GetHeading() const;
	FText GetTakeLabel() const;
	FText GetHint() const;
	FText GetResult() const;

	TWeakObjectPtr<AStageShiftCharacter> Player;
	TWeakObjectPtr<APlayerController> LocalController;
	TWeakObjectPtr<UAudioComponent> Music;
	TSharedPtr<SWidget> Panel;
	TArray<int32> SelectedTakes;
	TMap<TWeakObjectPtr<AActor>, FTransform> InitialTransforms;
	int32 CaseIndex = 0;
	FString Result;
	FVector WalkDirection = FVector::ZeroVector;
	float WalkLeft = 0.f;
	float InteractionLeft = 0.f;
	TWeakObjectPtr<AActor> MovingFixture;
	TWeakObjectPtr<USoundBase> ArrivalSound;
	FVector MoveFrom = FVector::ZeroVector;
	FVector MoveTo = FVector::ZeroVector;
	float MoveElapsed = 0.f;
	float MoveDuration = 0.f;
	TWeakObjectPtr<AActor> Curtain;
	float CurtainOpen = 0.f;
	float CurtainGoal = 0.f;
	FTimerHandle ResultTimer;
};
