#pragma once

#include "Components/Interaction/InteractionInterface.h"
#include "GameFramework/Actor.h"
#include "StageSelectionStation.generated.h"

class ACameraActor;
class UBoxComponent;
class USoundBase;
class UStageSelectionWidget;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;

USTRUCT(BlueprintType)
struct FStageSelectionOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TSoftObjectPtr<UWorld> Level;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	TObjectPtr<UStaticMesh> PreviewMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stage")
	bool bUnlocked = false;

	bool CanStart() const { return bUnlocked && !Level.IsNull(); }
};

UCLASS()
class STAGESHIFT_API AStageSelectionStation : public AActor, public IInteractionInterface
{
	GENERATED_BODY()

public:
	AStageSelectionStation();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;
	virtual const UMeshComponent* GetInteractionMesh() const override;
	virtual bool CanInteract_Implementation(const FInteractionContext& Context) const override;
	virtual EInteractionResult OnInteraction_Implementation(const FInteractionContext& Context) override;

	void BrowseStage(int32 Direction);
	bool TryStartStage();
	void CloseSelection();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Display;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionArea;

	UPROPERTY(EditInstanceOnly, Category = "Selection")
	TObjectPtr<ACameraActor> SelectionCamera;

	UPROPERTY(EditAnywhere, Category = "Selection")
	TSubclassOf<UStageSelectionWidget> WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Selection")
	TArray<FStageSelectionOption> Options;

	UPROPERTY(EditAnywhere, Category = "Selection", meta = (ClampMin = "0.0", Units = "s"))
	float CameraBlendTime = .65f;

	UPROPERTY(EditAnywhere, Category = "Selection|Rail")
	FVector PreviewOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Selection|Rail", meta = (ClampMin = "1.0", Units = "cm"))
	float RailTravelDistance = 265.f;

	UPROPERTY(EditAnywhere, Category = "Selection|Rail", meta = (ClampMin = "0.01", Units = "s"))
	float RailTravelTime = .42f;

	UPROPERTY(EditAnywhere, Category = "Selection|Audio")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditAnywhere, Category = "Selection|Audio")
	TObjectPtr<USoundBase> SlideSound;

	UPROPERTY(EditAnywhere, Category = "Selection|Audio")
	TObjectPtr<USoundBase> BackSound;

	UPROPERTY(EditAnywhere, Category = "Selection|Audio")
	TObjectPtr<USoundBase> ConfirmSound;

private:
	void RefreshSelectionWidget();
	void ReleaseSelection(bool bRestoreView);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PreviewA;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PreviewB;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CurrentPreview;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> IncomingPreview;

	UPROPERTY(Transient)
	TObjectPtr<UStageSelectionWidget> ActiveWidget;

	TWeakObjectPtr<AActor> PreviousCamera;
	TWeakObjectPtr<AActor> HiddenInteractor;
	int32 CurrentOptionIndex = 0;
	float SlideElapsed = 0.f;
	float SlideDirection = 0.f;
	bool bWasCursorVisible = false;
	bool bTravelRequested = false;
};
