#pragma once

#include "Components/ActorComponent.h"
#include "ThrowAimComponent.generated.h"

class ACharacter;
class APlayerController;
class UInteractionComponent;
class UPhysicsManipulationComponent;
class UUserWidget;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class STAGESHIFT_API UThrowAimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThrowAimComponent();
	void SetStickInput(FVector2D Value);
	void StopAiming();
	void FreezeAim();
	bool GetAimTarget(FVector& WorldLocation) const;
	FVector2D GetCursorPosition() const { return ScreenPosition; }
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(EditAnywhere, Category="Aim|Widget")
	TSubclassOf<UUserWidget> ReticleWidgetClass;

	UPROPERTY(EditAnywhere, Category="Aim|Widget", meta=(ClampMin="1.0"))
	FVector2D ReticleSize = FVector2D(32, 32);

	UPROPERTY(EditAnywhere, Category="Aim", meta=(ClampMin="0.0", ClampMax="0.9"))
	float StickDeadZone = 0.2f;

	UPROPERTY(EditAnywhere, Category="Aim", meta=(ClampMin="0.1"))
	float MouseSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Aim|Cursor", meta = (ClampMin = "1.0", ToolTip = "Cursor speed in pixels per second at 1080p."))
	float StickCursorSpeed = 900.0f;

	UPROPERTY(EditAnywhere, Category = "Aim|Cursor", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float StickCursorExponent = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Aim", meta = (ClampMin = "100.0", Units = "cm"))
	float AimTraceDistance = 100000.0f;

	UPROPERTY(EditAnywhere, Category = "Aim")
	TEnumAsByte<ECollisionChannel> AimTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category="Aim", meta=(ClampMin="1.0", Units="cm"))
	float AimDistance = 350.0f;

	UPROPERTY(EditAnywhere, Category="Aim", meta=(ClampMin="1.0"))
	float TurnSpeed = 12.0f;

private:
	void UpdateAim(APlayerController& Controller, float DeltaTime);
	bool GetViewportBounds(const APlayerController& Controller, FIntRect& Bounds, FVector2D& PlayerOrigin) const;
	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<UInteractionComponent> Interaction;
	TWeakObjectPtr<UPhysicsManipulationComponent> Manipulation;
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Reticle;
	FVector2D StickInput = FVector2D::ZeroVector;
	FVector2D StickDeviceSample = FVector2D::ZeroVector;
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	FVector2D LastMousePosition = FVector2D::ZeroVector;
	FVector AimTarget = FVector::ZeroVector;
	FVector AimDirection = FVector::ForwardVector;
	uint64 LastCursorUpdateFrame = MAX_uint64;
	bool bAiming = false;
	bool bWasPreparing = false;
	bool bGamepad = false;
	bool bStickMoved = false;
	bool bAimFrozen = false;
	bool bHasAimTarget = false;
	bool bHasScreenPosition = false;
	bool bSavedOrientToMovement = true;
	bool bSavedControllerYaw = false;
	bool bSavedControllerDesiredRotation = false;
};
