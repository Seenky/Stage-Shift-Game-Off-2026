#pragma once

#include "Components/StaticMeshComponent.h"
#include "StageControlLeverComponent.generated.h"

class UStageControlMeshComponent;
class UInteractionComponent;
enum class EInteractionHandMotion : uint8;

UCLASS(ClassGroup = (Stage), meta = (BlueprintSpawnableComponent))
class STAGESHIFT_API UStageControlLeverComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UStageControlLeverComponent();

	void BindVisual(UStageControlMeshComponent& Mesh, FName Bone, bool bTwist);
	void PlayHandMotion(UInteractionComponent& Interaction, EInteractionHandMotion Motion) const;
	void ReleaseHandGrip(UInteractionComponent& Interaction) const;
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
	TWeakObjectPtr<UStageControlMeshComponent> Visual;
	FName VisualBone;
	FVector TwistAxis = FVector::UpVector;
	bool bTwistVisual = false;
	FQuat RestRotation = FQuat::Identity;
	FVector PivotInParent = FVector::ZeroVector;
	FVector PivotInMesh = FVector::ZeroVector;
	FVector PullAxis = FVector::RightVector;
	float PullTime = 0.0f;
};
