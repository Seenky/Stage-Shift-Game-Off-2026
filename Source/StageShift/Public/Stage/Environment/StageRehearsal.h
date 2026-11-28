#pragma once

#include "CoreMinimal.h"
#include "Components/Interaction/InteractionInterface.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Physics/PhysicsActor.h"
#include "Stage/Environment/StageSoundControl.h"
#include "StageRehearsal.generated.h"

class AStageCurtainRail;
class AStageMemberCharacter;
class AStagePhysicsActor;
class AStageTurntableControl;
class ATriggerBox;
class UInputAction;
class UPrimitiveComponent;
class UStageCueComponent;
class UStageInputDeviceSubsystem;
class UStageShiftCharacterPreset;
class UStaticMeshComponent;
enum class EStageInputDevice : uint8;

UCLASS()
class UStageRehearsalProgress : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame) bool bInvitationSeen = false;
	UPROPERTY(SaveGame) bool bCompleted = false;
};

UENUM(BlueprintType)
enum class ERehearsalStep : uint8
{
	Outside, Available, Move, PickUp, Place, Throw, CloseCurtain, Turn, OpenCurtain,
	PerformerWalk, Sound, Outro, Complete
};

UCLASS()
class STAGESHIFT_API AStageRehearsal : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStageRehearsal();
	virtual void Tick(float DeltaSeconds) override;
	virtual const UMeshComponent* GetInteractionMesh() const override;
	virtual bool CanInteract_Implementation(const FInteractionContext& Context) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& Context) override;
	UFUNCTION(BlueprintPure, Category = "Rehearsal") ERehearsalStep GetStep() const { return Step; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Rehearsal") TObjectPtr<UStaticMeshComponent> ReplayStand;
	UPROPERTY(VisibleAnywhere, Category = "Rehearsal") TObjectPtr<UStageCueComponent> Cues;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Room") TObjectPtr<ATriggerBox> Room;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Room") TObjectPtr<AActor> Entrance;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Props") TObjectPtr<AActor> MoveMark;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Props") TObjectPtr<AActor> PlaceMark;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Props") TObjectPtr<AStagePhysicsActor> Parcel;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Props") TObjectPtr<AActor> Crate;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Stage") TObjectPtr<AStageCurtainRail> Curtain;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Stage") TObjectPtr<AStageTurntableControl> TurntableControl;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Stage") TObjectPtr<AStageSoundControl> SoundControl;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Stage") TSubclassOf<AStageMemberCharacter> PerformerClass;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Stage") TObjectPtr<UStageShiftCharacterPreset> PerformerAppearance;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Stage") TObjectPtr<AActor> PerformerStart;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Stage") TObjectPtr<AActor> PerformerMark;
	UPROPERTY(EditInstanceOnly, Category = "Rehearsal|Stage") FName KnockSoundId = TEXT("Knock");
	UPROPERTY(EditDefaultsOnly, Category = "Rehearsal|Input") TObjectPtr<UInputAction> InteractionAction;
	UPROPERTY(Transient) TObjectPtr<UStageRehearsalProgress> Progress;
	UPROPERTY(Transient) TObjectPtr<AStageMemberCharacter> Performer;

	UFUNCTION() void OnEnter(AActor* Zone, AActor* Actor);
	UFUNCTION() void OnExit(AActor* Zone, AActor* Actor);
	UFUNCTION() void OnParcelTransfer(AStagePhysicsActor* Prop, AActor* From, AActor* To, EStagePropTransferResult Result, float CatchSeconds);
	UFUNCTION() void OnParcelHit(UPrimitiveComponent* Component, AActor* Other, UPrimitiveComponent* OtherComponent,
	    FVector Impulse, const FHitResult& Hit);
	UFUNCTION() void OnSound(const FStageSoundWindow& Window, EStageSoundResult Result);
	bool IsConfigured() const;
	void InitializePlayer();
	void SaveProgress();
	void StartPractice();
	void ResetPractice();
	void ResetParcel();
	void ReleasePracticeInteraction() const;
	void SetStep(ERehearsalStep Next);
	void RefreshPrompt();
	void OnInputDeviceChanged(EStageInputDevice Device);
	void StartPerformer();
	void WalkPerformer();
	void UpdateThrow();
	bool IsHoldingParcel() const;
	FText InteractionKey() const;
	FText PromptDetails() const;

	TWeakObjectPtr<APawn> Player;
	TWeakObjectPtr<UStageInputDeviceSubsystem> InputDevice;
	FTransform ParcelHome;
	FBox RoomBounds;
	FTimerHandle InitializeTimer;
	FString SaveSlot;
	ERehearsalStep Step = ERehearsalStep::Outside;
	double ThrowAt = -1.0;
	double FinishAt = 0.0;
	bool bHitCrate = false;
	bool bShowingThrowTarget = false;
	bool bReady = false;
};
