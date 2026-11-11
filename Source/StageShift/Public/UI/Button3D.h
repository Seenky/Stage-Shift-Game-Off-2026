
#pragma once

#include "CoreMinimal.h"
#include "UIClickEventBase.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "Button3D.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UCLASS()
class STAGESHIFT_API AButton3D : public AActor
{
    GENERATED_BODY()

public:
    AButton3D();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Animation", meta = (ClampMin = "0.001"))
    float MaterialBlendTime = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Animation", meta = (ClampMin = "0.01"))
    float ActiveHoldTime = 0.25f;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Settings|Event")
    TObjectPtr<UUIClickEventBase> ClickEvent;

private:
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

    bool bIsHovered = false;
    bool bIsActive = false;

    FTimerHandle DeactivateTimerHandle;
};
