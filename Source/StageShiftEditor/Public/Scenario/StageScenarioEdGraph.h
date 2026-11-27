#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "StageScenarioEdGraph.generated.h"

class UStageDataAsset;
class UStageProductionDataAsset;
class UStageMemberCommand;
class UStageScenarioEdNode;
enum class EStageGraphAction : uint8
{
	Add,
	AddBranch,
	AddParallelRole,
	AddChild,
	AddReaction,
	Replace,
	WrapSequence,
	WrapParallel,
	Delete,
	Duplicate,
	Copy,
	Paste,
	Collapse,
	Expand,
	Enter,
	Rename,
	MoveEarlier,
	MoveLater,
	Validate,
	Layout,
	AddScene,
	AddPreparation,
	RemovePreparation,
	Fit,
	RunSequence,
	RunParallel,
	SetPrimaryBranch
};
UCLASS(Transient)

class STAGESHIFTEDITOR_API UStageScenarioEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TObjectPtr<UStageDataAsset> Scenario;
	UPROPERTY(Transient) TObjectPtr<UStageProductionDataAsset> Production;
	UPROPERTY(Transient) TObjectPtr<UStageMemberCommand> ScopeCommand;
	FGuid ScopeId;
	FGuid ActiveGroup;
	bool bLinear = true;
	bool bReadOnly = false;
	bool bInsertBefore = false;
	bool bReactions = false;
	bool bParallelAssignments = false;
	bool bReactionAssignments = false;
	int32 ScopeReaction = INDEX_NONE;
	int32 ScopeStep = INDEX_NONE;
	TFunction<void(EStageGraphAction, UClass*, UStageScenarioEdNode*, FVector2D)> OnAction;
	TFunction<bool(UStageScenarioEdNode*, UStageScenarioEdNode*, bool)> OnConnect;
	TFunction<void()> OnEdited;
	UObject* GetAsset() const;
	TMap<FGuid, FVector2D>& Positions() const;
	TArray<struct FStageGraphGroup>& Groups() const;
	TMap<FGuid, struct FStageGraphViewState>& Views() const;
	FGuid ViewKey() const;
	bool IsStorySteps() const;
};
