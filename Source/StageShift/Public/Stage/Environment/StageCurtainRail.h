#pragma once

#include "GameFramework/Actor.h"
#include "StageCurtainRail.generated.h"

class AStageManager;
class AStagePhysicsActor;
class ASkeletalMeshActor;
class USkeletalMeshComponent;
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
	bool CanDragLever() const;
	AActor* GetLever() const;
	bool IsOpen() const { return Progress >= 1.f; }
	UFUNCTION(BlueprintPure, Category = "Curtain")
	float GetOpenProgress() const { return Progress; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Curtain")
	TObjectPtr<USplineComponent> Rail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Curtain")
	TObjectPtr<UStaticMeshComponent> Track;
	UPROPERTY(EditInstanceOnly, Category = "Curtain")
	TSoftObjectPtr<AStagePhysicsActor> Lever;
	UPROPERTY(EditInstanceOnly, Category = "Curtain")
	TSoftObjectPtr<ASkeletalMeshActor> Curtain;

private:
	AActor* Resolve(const FSoftObjectPath& Reference) const;
	void ApplyCurtain() const;

	TWeakObjectPtr<AStageManager> StageManager;
	TWeakObjectPtr<AStagePhysicsActor> RuntimeLever;
	TWeakObjectPtr<USkeletalMeshComponent> CurtainMesh;
	FRotator LeverRotation = FRotator::ZeroRotator;
	float Progress = 0.f;
};
