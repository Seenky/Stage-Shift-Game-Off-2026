
#pragma once

#include "CoreMinimal.h"
#include "UIClickEventBase.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "Button3D.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;

UCLASS()

class STAGESHIFT_API AButton3D : public AActor
{
	GENERATED_BODY()

public:
	AButton3D();
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Button")
	void ActivateButton();

	int32 GetNavigationOrder() const { return NavigationOrder; }
	void SetNavigationFocus(bool bFocused, bool bUsePointer, bool bPlaySound = true);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ButtonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> ButtonRepresentationWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Materials")
	TObjectPtr<UMaterialInterface> InactiveButtonMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Materials")
	TObjectPtr<UMaterialInterface> HoveredButtonMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Materials")
	TObjectPtr<UMaterialInterface> ActiveButtonMaterial;

	// Empty keeps the original material slot 0 behavior.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Materials")
	FName FeedbackMaterialSlot;

	UPROPERTY(EditAnywhere, Category = "Settings|Materials")
	bool bTintLabelWithMaterial = false;

	UPROPERTY(EditAnywhere, Category = "Settings|Materials", meta = (EditCondition = "bTintLabelWithMaterial"))
	FLinearColor IdleLabelTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Settings|Materials", meta = (EditCondition = "bTintLabelWithMaterial"))
	FLinearColor FocusedLabelTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Animation", meta = (ClampMin = "0.001"))
	float MaterialBlendTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Settings|Animation", meta = (ClampMin = "0.0"))
	float FocusDepthOffset = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Animation", meta = (ClampMin = "0.01"))
	float ActiveHoldTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Settings|Event")
	TObjectPtr<UUIClickEventBase> ClickEvent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Event")
	bool bAllowUnboundClickFeedback = false;

	UPROPERTY(EditInstanceOnly, Category = "Settings|Navigation", meta = (ClampMin = "-1"))
	int32 NavigationOrder = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category = "Settings|Audio")
	TObjectPtr<USoundBase> HoverSound;

	UPROPERTY(EditAnywhere, Category = "Settings|Audio")
	TObjectPtr<USoundBase> PressSound;

private:
	int32 GetFeedbackMaterialIndex() const;

	UFUNCTION()
	void OnButtonClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	UFUNCTION()
	void OnButtonStartHover(UPrimitiveComponent* TouchedComponent);

	UFUNCTION()
	void OnButtonEndHover(UPrimitiveComponent* TouchedComponent);

	void DeactivateButton();
	void UpdateMaterialBlend(float DeltaTime);
	void RequestMaterialUpdate();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CurrentButtonMaterialDYN;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InactiveButtonMaterialDYN;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HoveredButtonMaterialDYN;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ActiveButtonMaterialDYN;

	float CurrentBlendState = 0.0f;
	FVector RestLocation = FVector::ZeroVector;

	bool bIsHovered = false;
	bool bNavigationFocused = false;
	bool bUsePointerFeedback = true;
	bool bIsActive = false;

	FTimerHandle DeactivateTimerHandle;
};
