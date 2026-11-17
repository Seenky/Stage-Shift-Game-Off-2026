// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/Interaction/InteractionInterface.h"
#include "GameFramework/Actor.h"
#include "StageDoor.generated.h"

UCLASS()
class STAGESHIFT_API AStageDoor : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStageDoor();

	virtual bool CanInteract_Implementation(const FInteractionContext& InteractionContext) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& InteractionContext) override;

	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> RootComp;

private:

	virtual void BeginPlay() override;

	enum class EDoorState : uint8
	{
		None,
		Open,
		Close
	};

	EDoorState CurrentDoorDirection = EDoorState::None;
	EDoorState CurrentDoorState = EDoorState::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess))
	FRotator RotationOffset;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess, ClampMin = 0.0001, UIMin = 0.0001))
	float OpenTime = 1.f;

	FQuat GetOpeningOffset(float Alpha) const;
	void SetDoorMoveAlpha(const float& InAlpha);

	float DoorMoveAlpha = 0.0f;
	FQuat ClosedRotation = FQuat::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess, ClampMin = 0, UIMin = 0, ClampMax = 1.0, UIMax = 1.0))
	float DebugDoorMoveAlpha = 0.0f;

	// Save the applied offset with the previewed transform so BeginPlay can restore the closed pose.
	UPROPERTY()
	FQuat AppliedPreviewOffset = FQuat::Identity;
};
