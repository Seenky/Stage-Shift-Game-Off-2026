#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/Interaction/InteractionOutlineStencil.h"
#include "InteractionOutlineComponent.generated.h"

class AStagePhysicsActor;
class UInteractionComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UPrimitiveComponent;

// One visual controller on the local player. No discovery or interaction decisions here.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))

class STAGESHIFT_API UInteractionOutlineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionOutlineComponent();
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	void SetPickupCueHighlight(UObject& Source, bool bEnabled);

	UPROPERTY(EditAnywhere, Category = "Outline")
	TObjectPtr<UMaterialInterface> OutlineMaterial;
	UPROPERTY(EditAnywhere, Category = "Outline")
	FLinearColor OutlineColor = FLinearColor(1.0f, 0.86f, 0.62f);
	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float OutlineThickness = 2.5f;
	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OutlineOpacity = 0.65f;
	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInDuration = 0.15f;
	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutDuration = 0.15f;
	UPROPERTY(EditAnywhere, Category = "Outline|Pulse")
	bool bEnableAppearPulse = false;
	UPROPERTY(EditAnywhere, Category = "Outline|Pulse", meta = (ClampMin = "0.0", Units = "s"))
	float AppearPulseDuration = 0.18f;
	UPROPERTY(EditAnywhere, Category = "Outline|Pulse", meta = (ClampMin = "0.0"))
	float AppearPulseAmplitude = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ClampMin = "0.0", Units = "cm"))
	float DepthBias = 1.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick) override;

private:
	TArray<FInteractionOutlineStencil> PrimitiveStates;
	TSet<TWeakObjectPtr<UObject>> PickupCueSources;
	TWeakObjectPtr<AActor> VisualTarget;
	TWeakObjectPtr<AStagePhysicsActor> FocusedProp;
	TWeakObjectPtr<UInteractionComponent> Interaction;
	FDelegateHandle CandidateChangedHandle;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MaterialInstance;
	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcess;
	float Alpha = 0.0f;
	float TransitionStartAlpha = 0.0f;
	float TransitionTime = 0.0f;
	float SelectionTime = 0.0f;
	bool bFadingOut = false;
	void OnCandidateChanged(AActor* Target);
	void UpdatePostProcess();
	void RestorePrimitives();
	void ClearVisual();
	void UpdateMaterial(float Thickness);
	void UpdatePickupCueMaterial();
	UFUNCTION()
	void OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
	UFUNCTION()
	void OnVisualTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
	UFUNCTION()
	void OnInteractionDeactivated(UActorComponent* Component);
};
