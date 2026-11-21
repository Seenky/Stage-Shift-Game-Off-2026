#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StageData.h"
#include "StructUtils/PropertyBag.h"
#include "StageProductionDataAsset.generated.h"

class UStageDataAsset;

UENUM(BlueprintType)
enum class EStageProductionState : uint8
{
	Idle,
	Preparing,
	Playing,
	Completed,
	Failed
};

USTRUCT(BlueprintType)

struct FStagePropPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FName PropId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") FName MarkId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0", Units = "cm")) float PositionTolerance = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") bool bCheckYaw = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0", ClampMax = "180", Units = "deg", EditCondition = "bCheckYaw"))
	float YawTolerance = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement") bool bRequireSettled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0", Units = "cm/s", EditCondition = "bRequireSettled")) float MaxSpeed = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0", Units = "deg/s", EditCondition = "bRequireSettled")) float MaxAngularSpeed = 30;
};

USTRUCT(BlueprintType)

struct FStageProductionScene
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") FName SceneId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scene") TObjectPtr<UStageDataAsset> Scenario;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preparation") bool bPrepareBefore = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preparation", meta = (EditCondition = "bPrepareBefore")) FText PreparationPrompt;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preparation", meta = (EditCondition = "bPrepareBefore")) bool bAutoStartWhenReady = false;
	// Zero means no countdown during preparation. This does not consume scene time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preparation", meta = (ClampMin = "0", Units = "s", EditCondition = "bPrepareBefore")) float PreparationTimeout = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preparation", meta = (EditCondition = "bPrepareBefore", TitleProperty = "PropId"))
	TArray<FStagePropPlacement> Placements;
#if WITH_EDITORONLY_DATA
	UPROPERTY() FGuid EditorGuid;
	UPROPERTY() FGuid PreparationGuid;
#endif
};

// Lives on the level-local manager, never on the reusable production asset.
USTRUCT(BlueprintType)

struct FStageCastPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast") TSubclassOf<AActor> ActorClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast") TSoftObjectPtr<AActor> BackstageMark;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cast") TSoftObjectPtr<AActor> StageMark;
};

USTRUCT(BlueprintType)

struct FStageSceneBindings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings") TMap<FName, TSoftObjectPtr<AActor>> Roles;
	// Includes scenario targets and preparation prop/mark IDs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings") TMap<FName, TSoftObjectPtr<AActor>> Targets;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings") TMap<FName, FStageCastPlacement> Cast;
};

USTRUCT()

struct FStageProductionDraft
{
	GENERATED_BODY()

	UPROPERTY() FGuid Id;
	UPROPERTY() FGuid Scope;
	UPROPERTY() FGuid Group;
	UPROPERTY() FGuid TargetScene;
	UPROPERTY() bool bPreparationOnly = false;
	UPROPERTY(EditAnywhere, Category = "Draft") FStageProductionScene Scene;
};

UCLASS(BlueprintType)

class STAGESHIFT_API UStageProductionDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Optional overrides of scene parameters with the same name and exact type.
	UPROPERTY(EditAnywhere, Category = "Graph Parameters") FInstancedPropertyBag Parameters;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production") FName ProductionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Production", meta = (TitleProperty = "SceneId")) TArray<FStageProductionScene> Scenes;
	bool ValidateDefinition(TArray<FString>& Errors) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
#if WITH_EDITORONLY_DATA
	UPROPERTY() TMap<FGuid, FVector2D> EditorPositions;
	UPROPERTY() TArray<FStageGraphGroup> EditorGroups;
	UPROPERTY() TArray<FStageProductionDraft> EditorDrafts;
	UPROPERTY() TMap<FGuid, FStageGraphViewState> EditorViews;
#endif
};
