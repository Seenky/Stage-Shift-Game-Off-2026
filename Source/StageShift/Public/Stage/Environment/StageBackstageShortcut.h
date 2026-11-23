#pragma once

#include "GameFramework/Actor.h"
#include "StageBackstageShortcut.generated.h"

class AStaticMeshActor;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class UStageCueSignalWidget;
class UTexture2D;
class UWidgetComponent;

UCLASS()
class STAGESHIFT_API AStageBackstageShortcut : public AActor
{
	GENERATED_BODY()

public:
	AStageBackstageShortcut();
	AStageBackstageShortcut* GetPartner() const;
	FVector GetExitFloorLocation() const;
	bool IsDiveEntering(const FVector& Previous, const FVector& Current) const;
	void SetNearby(bool bNearby);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	void ShowSignal(bool bShow, bool bEntry);
	void HighlightScreen(bool bHighlight);
	void UpdateCanvasSpeed(bool bNearby);

	UPROPERTY(VisibleAnywhere, Category = "Shortcut")
	TObjectPtr<UBoxComponent> Approach;
	UPROPERTY(VisibleAnywhere, Category = "Shortcut")
	TObjectPtr<UBoxComponent> Entrance;
	UPROPERTY(VisibleAnywhere, Category = "Shortcut")
	TObjectPtr<USceneComponent> Exit;
	UPROPERTY(VisibleAnywhere, Category = "Presentation")
	TObjectPtr<UWidgetComponent> Signal;
	UPROPERTY(EditInstanceOnly, Category = "Shortcut")
	TObjectPtr<AStageBackstageShortcut> Partner;
	UPROPERTY(EditInstanceOnly, Category = "Shortcut")
	TObjectPtr<AStaticMeshActor> Screen;
	UPROPERTY(EditAnywhere, Category = "Presentation")
	bool bMirrorEntryIcon = false;
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TObjectPtr<UTexture2D> EntryIcon;
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TObjectPtr<UTexture2D> ExitIcon;
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TObjectPtr<UMaterialInterface> HighlightMaterial;
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TObjectPtr<UMaterialParameterCollection> CanvasParameters;
	UPROPERTY(EditAnywhere, Category = "Presentation")
	TSoftClassPtr<UStageCueSignalWidget> SignalWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HighlightInstance;
};
