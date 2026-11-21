#pragma once

#include "Components/StaticMeshComponent.h"
#include "StageControlLeverComponent.generated.h"

UCLASS(ClassGroup = (Stage), meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UStageControlLeverComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UStageControlLeverComponent();

	FVector GetGripLocation() const;
	void SetTilt(const FQuat& Tilt);
	void PullToward(const FVector& WorldLocation);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

	// Put a LeverMount socket on the console mesh, or position the component by hand.
	UPROPERTY(EditAnywhere, Category = "Lever")
	bool bAutoMount = true;

	UPROPERTY(EditAnywhere, Category = "Lever")
	FName MountSocket = TEXT("LeverMount");

	// Console-local adjustment for a mesh without a LeverMount socket.
	UPROPERTY(EditAnywhere, Category = "Lever")
	FVector MountOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Lever")
	FName GripSocket = TEXT("InteractionGrip");

private:
	FQuat RestRotation = FQuat::Identity;
	FVector PivotInParent = FVector::ZeroVector;
	FVector PivotInMesh = FVector::ZeroVector;
	FVector PullAxis = FVector::RightVector;
	float PullTime = 0.0f;
};
