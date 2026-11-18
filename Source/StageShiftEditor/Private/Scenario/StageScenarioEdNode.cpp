#include "Scenario/StageScenarioEdNode.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "StageScenarioGraphAdapter.h"
#include "StageGraphOrganization.h"
#include "StageGraphDrafts.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "Stage/Core/StageMemberCommand.h"
#include "SGraphNode.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

const FName UStageScenarioEdNode::FailedPin(TEXT("On Failed"));
const FName UStageScenarioEdNode::TruePin(TEXT("True"));
const FName UStageScenarioEdNode::FalsePin(TEXT("False"));

class SStageScenarioNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SStageScenarioNode) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, UStageScenarioEdNode* Node)
	{
		GraphNode = Node;
		SetCursor(EMouseCursor::CardinalCross);
		UpdateGraphNode();
	}

	virtual void MoveTo(const FVector2f& Position, FNodeSet& Filter, bool bMarkDirty = true) override
	{
		auto* Graph = CastChecked<UStageScenarioEdGraph>(GraphNode->GetGraph());
		if (Graph->bReadOnly || (bMarkDirty && !FStageScenarioGraphAdapter::Backup(Graph->GetAsset()))) return;
		if (bMarkDirty && FStageScenarioGraphAdapter::Backup(Graph->GetAsset())) Graph->GetAsset()->Modify();
		SGraphNode::MoveTo(Position, Filter, bMarkDirty);
		if (bMarkDirty)
		{
			Graph->Positions().Add(GraphNode->NodeGuid, FVector2D(GraphNode->NodePosX, GraphNode->NodePosY));
			Graph->GetAsset()->MarkPackageDirty();
		}
	}

	virtual TSharedRef<SWidget> CreateNodeContentArea() override
	{
		auto Box = SNew(SVerticalBox);
		auto* Node = CastChecked<UStageScenarioEdNode>(GraphNode);
		auto* Graph = CastChecked<UStageScenarioEdGraph>(Node->GetGraph());
		if (!Node->Summary.IsEmpty())
			Box->AddSlot()
			.AutoHeight()
			.Padding(8, 4)
			[
				SNew(SBox)
				.MaxDesiredWidth(260)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Node->Summary))
					.AutoWrapText(true)
				]
			];
		if (Graph->Scenario && (Node->Command || Node->Kind == EStageScenarioNodeKind::Reaction))
		{
			auto AddRole = [&](bool bSubject)
			{
				Box->AddSlot()
				.AutoHeight()
				.Padding(6, 2)
				[
					SNew(SComboButton)
					.IsEnabled(!Graph->bReadOnly && !Graph->ScopeCommand)
					.OnGetMenuContent_Lambda([Graph, Node, bSubject]
					{
						FMenuBuilder Menu(true, nullptr);
						auto Roles = Graph->Scenario->GetRoleOptions();
						if (bSubject) Roles.Insert(NAME_None, 0);
						for (FName Id : Roles)
							Menu.AddMenuEntry(FText::FromString(Id.IsNone() ? TEXT("Any") : Id.ToString()), FText(), FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([Graph, Node, Id, bSubject]
								{
									const bool Changed = Node->IsDraft()
										? FStageGraphDrafts::SetRole(Graph, Node, Id, bSubject)
										: FStageScenarioGraphAdapter(Graph->Scenario).SetRole(Node, Id, bSubject);
									if (Changed && Graph->OnEdited) Graph->OnEdited();
								})));
						return Menu.MakeWidget();
					})
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text_Lambda([Graph, Node, bSubject]
						{
							FName Id;
							if (Node->IsDraft() && Graph->Scenario->EditorDrafts.IsValidIndex(Node->DraftIndex))
							{
								const auto& D = Graph->Scenario->EditorDrafts[Node->DraftIndex];
								Id = D.Kind == EStageGraphDraftKind::Reaction
									? (bSubject ? D.Reaction.SubjectRoleId : D.Reaction.GetEffectiveRoleId())
									: D.Step.Commands.IsEmpty() ? NAME_None : D.Step.Commands[0].GetEffectiveRoleId();
							}
							else if (Graph->Scenario->MemberReactions.IsValidIndex(Node->ReactionIndex))
							{
								const auto& R = Graph->Scenario->MemberReactions[Node->ReactionIndex];
								Id = bSubject ? R.SubjectRoleId
									: Node->AssignmentIndex > 0 && R.AdditionalPerformers.IsValidIndex(Node->AssignmentIndex - 1)
										? R.AdditionalPerformers[Node->AssignmentIndex - 1].GetEffectiveRoleId()
										: R.GetEffectiveRoleId();
							}
							else if (auto* A = FStageScenarioGraphAdapter(Graph->Scenario).Assignment(Node))
								Id = A->GetEffectiveRoleId();
							return FText::FromString((bSubject ? TEXT("Subject: ") : TEXT("Role: ")) + (Id.IsNone() ? TEXT("Any") : Id.ToString()));
						})
					]
				];
			};
			AddRole(false);
			if (Node->Kind == EStageScenarioNodeKind::Reaction) AddRole(true);
		}
		Box->AddSlot()
		.AutoHeight()
		[
			SGraphNode::CreateNodeContentArea()
		];
		return Box;
	}
};

FLinearColor UStageScenarioEdNode::GetNodeTitleColor() const
{
	switch (Kind)
	{
	case EStageScenarioNodeKind::Start:
		return FLinearColor(0.65f, 0.035f, 0.025f);
	case EStageScenarioNodeKind::Reaction:
		return FLinearColor(0.55f, 0.22f, 0.08f);
	case EStageScenarioNodeKind::Step:
		return FLinearColor(0.06f, 0.32f, 0.42f);
	case EStageScenarioNodeKind::Branch:
		return FLinearColor(0.20f, 0.30f, 0.48f);
	case EStageScenarioNodeKind::Group:
		return FLinearColor(0.16f, 0.23f, 0.45f);
	case EStageScenarioNodeKind::Scene:
		return FLinearColor(0.06f, 0.32f, 0.42f);
	case EStageScenarioNodeKind::Preparation:
		return FLinearColor(0.5f, 0.3f, 0.06f);
	case EStageScenarioNodeKind::Finale:
		return FLinearColor(0.3f, 0.18f, 0.5f);
	default:
		return FLinearColor(0.14f, 0.25f, 0.2f);
	}
}

void UStageScenarioEdNode::AllocateDefaultPins()
{
	if (Kind != EStageScenarioNodeKind::Start && Kind != EStageScenarioNodeKind::Reaction) CreatePin(EGPD_Input, TEXT("StageOrder"), TEXT("Execute"));
	if (Kind == EStageScenarioNodeKind::Branch)
	{
		CreatePin(EGPD_Output, TEXT("StageOrder"), TruePin);
		CreatePin(EGPD_Output, TEXT("StageOrder"), FalsePin);
		return;
	}
	if (Kind != EStageScenarioNodeKind::Return)
		CreatePin(EGPD_Output, TEXT("StageOrder"), Kind == EStageScenarioNodeKind::Start || Kind == EStageScenarioNodeKind::Reaction ? TEXT("Execute") : TEXT("Then"));
}

FName UStageScenarioEdNode::GetExitPinName(bool bAlternate) const
{
	if (Kind == EStageScenarioNodeKind::Branch) return bAlternate ? FalsePin : TruePin;
	return bAlternate ? FailedPin : FName(TEXT("Then"));
}

TSharedPtr<SGraphNode> UStageScenarioEdNode::CreateVisualWidget()
{
	return SNew(SStageScenarioNode, this);
}

FText UStageScenarioEdNode::GetNodeTitle(ENodeTitleType::Type Type) const
{
	if (Type == ENodeTitleType::EditableTitle)
	{
		const auto* Graph = CastChecked<UStageScenarioEdGraph>(GetGraph());
		if (Graph->Scenario && bStepRoot && Kind != EStageScenarioNodeKind::Group && Graph->Scenario->Steps.IsValidIndex(StepIndex))
			return FText::FromName(Graph->Scenario->Steps[StepIndex].StepId);
		if (Graph->Production && Kind == EStageScenarioNodeKind::Scene && Graph->Production->Scenes.IsValidIndex(StepIndex))
			return FText::FromName(Graph->Production->Scenes[StepIndex].SceneId);
	}
	return Title;
}

void UStageScenarioEdNode::OnRenameNode(const FString& Name)
{
	auto* Graph = CastChecked<UStageScenarioEdGraph>(GetGraph());
	if (!Graph->bReadOnly && FStageGraphOrganization::Rename(Graph, this, Name) && Graph->OnEdited) Graph->OnEdited();
}
