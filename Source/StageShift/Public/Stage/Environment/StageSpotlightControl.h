#pragma once

#include "Components/Interaction/InteractionInterface.h"
#include "GameFramework/Actor.h"
#include "StageSpotlightControl.generated.h"

class USpotLightComponent;
class UStageControlLeverComponent;
class UStageControlMeshComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EStageSpotlightState : uint8
{
	Idle, Aiming, Ready, TargetLit, TargetLost
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStageSpotlightChanged, FName, CueId, AActor*, Target, EStageSpotlightState, State);

UCLASS(Blueprintable)
class STAGESHIFT_API AStageSpotlightControl : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStageSpotlightControl();
	virtual const UMeshComponent* GetInteractionMesh() const override;
	virtual bool CanInteract_Implementation(const FInteractionContext& Context) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& Context) override;
	virtual FName GetInteractionAnimationId_Implementation() const override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Stage|Light")
	void SetCueTarget(FName InCueId, AActor* InTarget);

	bool ClaimCue(UObject& Requester, FName InCueId, AActor* InTarget);
	void ReleaseCue(const UObject& Requester, bool bSucceeded);

	UFUNCTION(BlueprintCallable, Category = "Stage|Light")
	void CommitAim();
	UFUNCTION(BlueprintPure, Category = "Stage|Light")
	EStageSpotlightState GetSpotlightState() const { return State; }

	UPROPERTY(BlueprintAssignable, Category = "Stage|Light")
	FStageSpotlightChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Stage|Audio")
	TObjectPtr<class USoundBase> ActivationSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStaticMeshComponent> ControlMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStageControlLeverComponent> ControlLever;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UStageControlMeshComponent> ControlVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage|Light")
	TObjectPtr<USpotLightComponent> Spotlight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Light")
	TSoftObjectPtr<AActor> InitialTarget;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage|Light")
	FName InitialCueId;
	UPROPERTY(EditAnywhere, Category = "Stage|Light")
	FVector TargetOffset = FVector(0, 0, 30);
	UPROPERTY(EditAnywhere, Category = "Stage|Light")
	FVector2D YawLimits = FVector2D(-60, 60);
	UPROPERTY(EditAnywhere, Category = "Stage|Light")
	FVector2D PitchLimits = FVector2D(-35, 30);
	UPROPERTY(EditAnywhere, Category = "Stage|Light", meta = (ClampMin = "0.0"))
	float MouseDegreesPerUnit = 0.18f;
	UPROPERTY(EditAnywhere, Category = "Stage|Light", meta = (ClampMin = "0.0"))
	float StickDegreesPerSecond = 70.f;
	UPROPERTY(EditAnywhere, Category = "Stage|Light", meta = (ClampMin = "1.0"))
	float AimingIntensityMultiplier = 1.75f;
	UPROPERTY(EditAnywhere, Category = "Stage|Light", meta = (ClampMin = "0.0", Units = "deg"))
	float TargetGraceAngle = 5.f;
	UPROPERTY(EditAnywhere, Category = "Stage|Light", meta = (ClampMin = "1.0", Units = "cm"))
	float OperatorRange = 220.f;
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Stage|Light")
	EStageSpotlightState State = EStageSpotlightState::Idle;

private:
	void ApplyCueTarget(FName InCueId, AActor* InTarget);
	void ReleaseAimOperator();
	void SetState(EStageSpotlightState Value);
	void UpdateLightIntensity(float DeltaSeconds);
	void CheckTarget();
	FRotator BaseRotation;
	float BaseIntensity = 0.f;
	float LightAlpha = 0.f;
	float AimingIntensityAlpha = 0.f;
	double KeepLightUntil = 0.0;
	bool bAimCommittedForCue = false;
	FVector2D AimOffset = FVector2D::ZeroVector;
	TOptional<FVector2D> LastCursorPosition;
	TWeakObjectPtr<AActor> AimOperator;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<UObject> CueOwner;
	FName CueId;
};
