// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InteractionTypes.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STAGESHIFT_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();
	
	UFUNCTION(BlueprintCallable, Category = "Interaction Component")
	EInteractionResult BeginInteraction(const FInteractionContext& InteractionContext, FInteractionPayload& OutPayload);
	
	UFUNCTION(BlueprintCallable, Category = "Interaction Component")
	bool EndInteraction();

	UFUNCTION(BlueprintCallable, Category = "Interaction Component")
	void CancelInteraction();
	
	UFUNCTION(BlueprintPure, Category = "Interaction Component")
	const FInteractionPayload& GetActiveInteraction() const { return ActiveInteraction; }

	UFUNCTION(BlueprintPure, Category = "Interaction Component")
	bool HasActiveInteraction() const { return ActiveInteraction.IsValid(); }
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float CapsuleRadius = 44.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = 0.0f, UIMin =0.0f))
	float CapsuleHalfHeight = 88.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float InteractionLength = 44.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
	
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
private:
	UPROPERTY(Transient)
	FInteractionPayload ActiveInteraction;

	UPROPERTY(Transient)
	FInteractionContext ActiveContext;

	EInteractionResult ActiveResult = EInteractionResult::Rejected;
	double PressStartTime = 0.0;
	bool bDispatchingInteraction = false;
	bool bEndingPlay = false;

	bool FinishInteraction(bool bCancelled);
	void ClearActiveInteraction();

	UFUNCTION()
	void OnTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);
};
