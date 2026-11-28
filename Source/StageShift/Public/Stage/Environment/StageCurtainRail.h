#pragma once

#include "GameFramework/Actor.h"
#include "StageCurtainRail.generated.h"

class AStageManager;
class AStagePhysicsActor;
class ASkeletalMeshActor;
class UAudioComponent;
class UMeshComponent;
class USkeletalMeshComponent;
class USoundBase;
class UStageControlMeshComponent;
class USplineComponent;
class UStaticMeshComponent;

UCLASS()
class STAGESHIFT_API AStageCurtainRail : public AActor
{
	GENERATED_BODY()

public:
	AStageCurtainRail();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetStageManager(AStageManager* Manager);
	void SetPracticeControlEnabled(bool bEnabled) { bPracticeControlEnabled = bEnabled; }
	void ResetPracticePosition(bool bOpen);
	bool CanDragLever() const;
	AActor* GetLever() const;
	const UMeshComponent* GetControlVisual() const;
	bool IsOpen() const { return Progress >= 1.f; }
	UFUNCTION(BlueprintPure, Category = "Curtain")
	float GetOpenProgress() const { return Progress; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Curtain")
	TObjectPtr<USplineComponent> Rail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Curtain")
	TObjectPtr<UStaticMeshComponent> Track;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Curtain")
	TObjectPtr<UStageControlMeshComponent> ControlVisual;
	UPROPERTY(EditInstanceOnly, Category = "Curtain")
	TSoftObjectPtr<AStagePhysicsActor> Lever;
	UPROPERTY(EditInstanceOnly, Category = "Curtain")
	TSoftObjectPtr<ASkeletalMeshActor> Curtain;
	UPROPERTY(VisibleAnywhere, Category = "Curtain|Audio")
	TObjectPtr<UAudioComponent> LeverMovementAudio;
	UPROPERTY(EditAnywhere, Category = "Curtain|Audio")
	TObjectPtr<USoundBase> LeverStopSound;

private:
	AActor* Resolve(const FSoftObjectPath& Reference) const;
	void ApplyCurtain() const;
	void UpdateSliderVisual() const;
	void UpdateLeverAudio(bool bCanSlide, float Speed, const FVector& Location, float DeltaSeconds);

	TWeakObjectPtr<AStageManager> StageManager;
	TWeakObjectPtr<AStagePhysicsActor> RuntimeLever;
	TWeakObjectPtr<USkeletalMeshComponent> CurtainMesh;
	FRotator LeverRotation = FRotator::ZeroRotator;
	float Progress = 0.f;
	float LeverAudioSpeed = 0.f;
	TOptional<float> LeverAudioStop = 0.f;
	bool bLeverAudible = false;
	bool bPracticeControlEnabled = false;
};
