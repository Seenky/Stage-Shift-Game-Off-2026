#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Stage/Cues/StageCueTypes.h"
#include "StageCueWorldMarker.generated.h"

class UMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

// Blueprint supplies materials and palette. Never borrows the interaction stencil.
UCLASS(Blueprintable)
class STAGESHIFT_API AStageCueWorldMarker : public AActor
{
	GENERATED_BODY()

public:
	AStageCueWorldMarker();
	void Configure(AActor* Target, const FStageCueTarget& Definition, const FStageCueView& Cue);
	FVector GetIndicatorLocation() const;
	bool HasValidTarget() const;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Cue") FStageCueView CueView;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Cue") EStageCueMarkerType MarkerType = EStageCueMarkerType::Object;
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Cue") FLinearColor Tint;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UMaterialInterface> ObjectMaterial;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UStaticMesh> ZoneMesh;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UMaterialInterface> ZoneMaterial;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor TaskColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor ProblemColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor UpcomingColor = FLinearColor::White;

protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void ReleaseHighlight();
	TWeakObjectPtr<AActor> TrackedActor;
	TArray<TWeakObjectPtr<UMeshComponent>> HighlightedMeshes;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ZoneVisual;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ObjectInstance;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ZoneInstance;
	FVector TargetOffset = FVector::ZeroVector;
};
