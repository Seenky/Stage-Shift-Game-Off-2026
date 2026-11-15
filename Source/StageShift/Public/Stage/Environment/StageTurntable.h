#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageTurntable.generated.h"

class UStaticMeshComponent;

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
	int32 GetCurrentSector() const { return CurrentSector; }

protected:
	virtual void BeginPlay() override;

private:
	FRotator InitialRotation;
	float RotationElapsed = 0.0f;
	float StartYaw = 0.0f;
	float EndYaw = 0.0f;
	int32 CurrentSector = 0;
	int32 TargetSector = 0;
	bool bRotating = false;
};
