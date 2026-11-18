#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "StageScenarioEdNode.generated.h"

class UStageMemberCommand;
enum class EStageScenarioNodeKind : uint8
{
	Start,
	Step,
	Branch,
	Command,
	Reaction,
	Finale,
	Return,
	Group,
	Scene,
	Preparation
};
UCLASS(Transient)

class STAGESHIFTEDITOR_API UStageScenarioEdNode : public UEdGraphNode
{
	GENERATED_BODY()

public:
	static const FName FailedPin;
	static const FName TruePin;
	static const FName FalsePin;
	static bool IsAlternateExit(FName Pin) { return Pin == FailedPin || Pin == FalsePin; }
	FName GetExitPinName(bool bAlternate) const;

	EStageScenarioNodeKind Kind = EStageScenarioNodeKind::Command;
	int32 StepIndex = INDEX_NONE;
	int32 AssignmentIndex = INDEX_NONE;
	int32 ReactionIndex = INDEX_NONE;
	int32 ChildIndex = INDEX_NONE;
	int32 DraftIndex = INDEX_NONE;

	bool IsDraft() const { return DraftIndex != INDEX_NONE; }
	UPROPERTY(Transient)
	TObjectPtr<UStageMemberCommand> Command;
	UPROPERTY(Transient)
	TObjectPtr<UStageMemberCommand> ParentCommand;
	FText Title;
	FString Summary;
	FGuid GroupId;
	TArray<FGuid> RepresentedIds;
	bool bStepRoot = false;
	bool bParallel = false;
	virtual FText GetNodeTitle(ENodeTitleType::Type Type) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void AllocateDefaultPins() override;
	virtual TSharedPtr<class SGraphNode> CreateVisualWidget() override;

	virtual bool CanUserDeleteNode() const override
	{
		return Kind != EStageScenarioNodeKind::Start && Kind != EStageScenarioNodeKind::Return && Kind != EStageScenarioNodeKind::Finale;
	}

	virtual bool CanDuplicateNode() const override { return CanUserDeleteNode(); }

	virtual void OnRenameNode(const FString& NewName) override;
};
