#pragma once
#include "CoreMinimal.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Scenario/StageScenarioEdNode.h"

class UStageScenarioEdGraph;

// All semantic graph edits write canonical scenario definitions in one transaction.
class FStageScenarioGraphAdapter
{
public:
	explicit FStageScenarioGraphAdapter(UStageDataAsset* InData) : Data(InData) {}

	void Build(UStageScenarioEdGraph* Graph, bool bReactions) const;
	void BuildView(UStageScenarioEdGraph* Graph) const;
	static FGuid StepKey(const UStageDataAsset* Asset, int32 Index);
	static FGuid ReactionKey(const UStageDataAsset* Asset, int32 Index);
	bool SetContainerMode(const UStageScenarioEdNode* Node, UClass* Class);
	void EnsureIdentities();
	bool AddTypedStep(UClass* Class, FName RoleId, int32 AfterIndex = INDEX_NONE);
	bool AddTypedReaction(UClass* Class, FName RoleId);
	bool AddInView(UStageScenarioEdGraph* Graph, UClass* Class, const UStageScenarioEdNode* From, FVector2D Position);
	bool Wrap(const UStageScenarioEdNode* Node, UClass* Container);
	bool SetTarget(const UStageScenarioEdNode* Node, FName TargetId);
	bool MoveElementsAfter(const UStageScenarioEdGraph* Graph, const UStageScenarioEdNode* Node, const UStageScenarioEdNode* After);
	bool AddStep(FName RoleId);
	bool AddReaction(FName RoleId);
	bool AddCommand(const UStageScenarioEdNode* Node, UClass* Class, FName RoleId, bool bChild);
	bool ReplaceCommand(const UStageScenarioEdNode* Node, UClass* Class);
	bool Delete(const UStageScenarioEdNode* Node);
	bool Duplicate(const UStageScenarioEdNode* Node);
	bool Reorder(const UStageScenarioEdNode* Node, int32 Delta);
	bool MoveStepAfter(int32 StepIndex, int32 AfterIndex);
	bool SetRole(const UStageScenarioEdNode* Node, FName RoleId, bool bSubject = false);
	static bool Backup(UObject* Data);
	static void RenewGuids(UStageMemberCommand* Command);
	static FGuid Key(const UObject* Object);
	FStageMemberAssignment* Assignment(const UStageScenarioEdNode* Node) const;

private:
	friend struct FStageGraphDrafts;
	UStageDataAsset* Data;
	UStageMemberCommand* NewCommand(UClass* Class, UObject* Outer) const;
	TObjectPtr<UStageMemberCommand>* CommandSlot(const UStageScenarioEdNode* Node) const;
	TArray<TObjectPtr<UStageMemberCommand>>* Children(UStageMemberCommand* Parent) const;
	void Changed() const;
};
