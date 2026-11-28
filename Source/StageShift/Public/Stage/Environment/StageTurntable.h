#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageTurntable.generated.h"

class AStagePhysicsActor;
class UAudioComponent;
class UStaticMeshComponent;
class AStageManager;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStageTurntableArrived, int32, SectorIndex);

/** A moving floor. Attach scenery to this actor; controls and backstage stay outside it. */
UCLASS(Blueprintable)
class STAGESHIFT_API AStageTurntable : public AActor
{
	GENERATED_BODY()

public:
	AStageTurntable();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Turntable")
	TObjectPtr<UStaticMeshComponent> Platform;

	UPROPERTY(EditInstanceOnly, Category = "Turntable", meta = (ToolTip = "Scenery in loaded sublevels that should rotate with the stage."))
	TArray<TSoftObjectPtr<AActor>> SceneryToRotate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turntable", meta = (ClampMin = "2", ClampMax = "12"))
	int32 SectorCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turntable", meta = (ClampMin = "0.1", Units = "s"))
	float RotationDuration = 6.0f;

	UPROPERTY(BlueprintAssignable, Category = "Turntable")
	FStageTurntableArrived OnArrived;

	UFUNCTION(BlueprintCallable, Category = "Turntable")
	bool RotateNext();

	UFUNCTION(BlueprintCallable, Category = "Turntable")
	bool RotateToSector(int32 SectorIndex);

	UFUNCTION(BlueprintPure, Category = "Turntable")
	bool IsRotating() const { return bRotating; }

	UFUNCTION(BlueprintPure, Category = "Turntable")
	bool IsNavigationReady() const;

	UFUNCTION(BlueprintPure, Category = "Turntable")
	int32 GetCurrentSector() const { return CurrentSector; }
	bool CanRotate() const;
	void SetStageManager(AStageManager* Manager);
	void ResetPracticePosition();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Turntable|Audio")
	TObjectPtr<USoundBase> RotationSound;

	UPROPERTY(EditAnywhere, Category = "Turntable|Audio", meta = (ToolTip = "Looping sound played while the platform rotates."))
	TObjectPtr<USoundBase> RotationLoopSound;

	UPROPERTY(EditAnywhere, Category = "Turntable|Audio")
	TObjectPtr<USoundBase> RotationStopSound;

private:
	struct FSecuredProp
	{
		TWeakObjectPtr<AStagePhysicsActor> Prop;
		ECollisionEnabled::Type Collision;
	};

	void AttachScenery();
	void SecureProps();
	void ReleaseProps();

	TWeakObjectPtr<AStageManager> StageManager;
	TWeakObjectPtr<UAudioComponent> RotationAudio;
	TArray<FSecuredProp> SecuredProps;
	FRotator InitialRotation;
	float RotationElapsed = 0.0f;
	float StartYaw = 0.0f;
	float EndYaw = 0.0f;
	int32 CurrentSector = 0;
	int32 TargetSector = 0;
	bool bRotating = false;
};
