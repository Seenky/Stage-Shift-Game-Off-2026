#include "Scenario/StageScenarioGraphSchema.h"
#include "Scenario/StageScenarioEdNode.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "StageScenarioGraphAdapter.h"
#include "StageCommandCatalog.h"
#include "StageGraphDrafts.h"
#include "Stage/Core/StageMemberCommand.h"
#include "ToolMenu.h"

namespace
{
	struct FStageMenuAction : FEdGraphSchemaAction
	{
		EStageGraphAction Action;
		FSoftClassPath ClassPath;

		FStageMenuAction(FString Category, FText Label, EStageGraphAction InAction, FSoftClassPath InClass = {})
		    : FEdGraphSchemaAction(FText::FromString(Category), Label, FText(), 0), Action(InAction), ClassPath(InClass)
		{
		}

		virtual UEdGraphNode* PerformAction(UEdGraph* Parent, UEdGraphPin* From, const FVector2f& Location, bool bSelect = true) override
		{
			auto* Graph = CastChecked<UStageScenarioEdGraph>(Parent);
			if (Graph->bReadOnly) return nullptr;
			UClass* Class = ClassPath.IsNull() ? nullptr : ClassPath.TryLoadClass<UObject>();
			auto* Node = From ? Cast<UStageScenarioEdNode>(From->GetOwningNode()) : nullptr;
			Graph->bInsertBefore = From && From->Direction == EGPD_Input;
			if (Graph->OnAction) Graph->OnAction(Action, Class, Node, FVector2D(Location));
			Graph->bInsertBefore = false;
			return nullptr;
		}
	};
}

void UStageScenarioGraphSchema::SetNodePosition(UEdGraphNode* Node, const FVector2f& Position) const
{
	auto* Graph = Node ? Cast<UStageScenarioEdGraph>(Node->GetGraph()) : nullptr;
	if (!Graph || Graph->bReadOnly || !FStageScenarioGraphAdapter::Backup(Graph->GetAsset())) return;
	// Q / native alignment use the schema directly, bypassing SGraphNode::MoveTo.
	// Capture the owning asset in that same alignment transaction before updating its layout.
	Graph->GetAsset()->Modify();
	Super::SetNodePosition(Node, FVector2f(FMath::RoundToFloat(Position.X), FMath::RoundToFloat(Position.Y)));
	Graph->Positions().Add(Node->NodeGuid, FVector2D(Node->NodePosX, Node->NodePosY));
	Graph->GetAsset()->MarkPackageDirty();
}

void UStageScenarioGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& Builder) const
{
	const auto* Graph = Cast<UStageScenarioEdGraph>(Builder.CurrentGraph);
	if (!Graph || Graph->bReadOnly) return;
	if (Graph->Production)
		Builder.AddAction(MakeShared<FStageMenuAction>(TEXT("Production"), FText::FromString(TEXT("Scene")), EStageGraphAction::AddScene));
	else
	{
		if (Graph->IsStorySteps()) Builder.AddAction(MakeShared<FStageMenuAction>(TEXT("Graph"), FText::FromString(TEXT("Branch")), EStageGraphAction::AddBranch));
		EStageGraphAction Action = Graph->bReactions && !Graph->ScopeCommand && !Graph->bReactionAssignments ? EStageGraphAction::AddReaction : EStageGraphAction::Add;
		for (const auto& Type : FStageCommandCatalog::Get())
			Builder.AddAction(MakeShared<FStageMenuAction>(Type.Category, Type.Name, Action, Type.Path));
	}
	Builder.AddAction(MakeShared<FStageMenuAction>(TEXT("Edit"), FText::FromString(TEXT("Paste")), EStageGraphAction::Paste));
	Builder.AddAction(MakeShared<FStageMenuAction>(TEXT("Edit"), FText::FromString(TEXT("Collapse selected nodes to subgraph")), EStageGraphAction::Collapse));
	Builder.AddAction(MakeShared<FStageMenuAction>(TEXT("Graph"), FText::FromString(TEXT("Arrange visible nodes")), EStageGraphAction::Layout));
	Builder.AddAction(MakeShared<FStageMenuAction>(TEXT("Graph"), FText::FromString(TEXT("Frame all nodes")), EStageGraphAction::Fit));
	Builder.AddAction(MakeShared<FStageMenuAction>(TEXT("Graph"), FText::FromString(TEXT("Validate")), EStageGraphAction::Validate));
}

void UStageScenarioGraphSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	auto* Graph = Cast<UStageScenarioEdGraph>(const_cast<UEdGraph*>(Context->Graph.Get()));
	auto* Node = Cast<UStageScenarioEdNode>(const_cast<UEdGraphNode*>(Context->Node.Get()));
	if (!Graph || !Node || Graph->bReadOnly) return;
	auto& Section = Menu->AddSection(TEXT("StageNode"), FText::FromString(TEXT("Node")));
	auto Add = [&](const TCHAR* Label, EStageGraphAction Action)
	{
		Section.AddMenuEntry(NAME_None, FText::FromString(Label), FText(), FSlateIcon(),
		    FUIAction(FExecuteAction::CreateLambda([Graph, Node, Action]
		{
			if (Graph->OnAction) Graph->OnAction(Action, nullptr, Node, FVector2D(Node->NodePosX, Node->NodePosY));
		})));
	};
	if (Node->Kind == EStageScenarioNodeKind::Group)
	{
		Add(TEXT("Open subgraph"), EStageGraphAction::Enter);
		Add(TEXT("Expand subgraph"), EStageGraphAction::Expand);
	}
	if (Node->Kind == EStageScenarioNodeKind::Group || Node->bStepRoot || Node->Kind == EStageScenarioNodeKind::Scene) Add(TEXT("Rename"), EStageGraphAction::Rename);
	auto Submenu = [&](const TCHAR* Label, EStageGraphAction Action)
	{
		Section.AddSubMenu(NAME_None, FText::FromString(Label), FText(),
		    FNewToolMenuDelegate::CreateLambda([Graph, Node, Action](UToolMenu* Sub)
		{
			auto& Items = Sub->AddSection(TEXT("Commands"));
			for (const auto& Type : FStageCommandCatalog::Get())
				Items.AddMenuEntry(NAME_None, Type.Name, FText(), FSlateIcon(),
				    FUIAction(FExecuteAction::CreateLambda([Graph, Node, Action, Path = Type.Path]
				{
					if (auto* Class = Path.TryLoadClass<UObject>(); Class && Graph->OnAction) Graph->OnAction(Action, Class, Node, FVector2D(Node->NodePosX, Node->NodePosY));
				})));
		}));
	};
	if (Graph->Scenario && Node->Kind != EStageScenarioNodeKind::Group && Node->Kind != EStageScenarioNodeKind::Branch && (Node->bStepRoot || Node->Kind == EStageScenarioNodeKind::Finale))
		Submenu(TEXT("Add parallel performer"), EStageGraphAction::AddParallelRole);
	if (Node->Kind == EStageScenarioNodeKind::Reaction && !Node->IsDraft())
	{
		Add(TEXT("Edit reaction performers"), EStageGraphAction::Enter);
		Submenu(TEXT("Add action for primary role"), EStageGraphAction::AddChild);
		Submenu(TEXT("Add another reacting role"), EStageGraphAction::AddParallelRole);
		Add(TEXT("Primary role actions: Sequence"), EStageGraphAction::RunSequence);
		Add(TEXT("Primary role actions: Parallel"), EStageGraphAction::RunParallel);
	}
	if (Node->Kind == EStageScenarioNodeKind::Step || Node->Kind == EStageScenarioNodeKind::Finale || (Node->Command && Node->Command->IsComposite()))
		Add(TEXT("Open contents"), EStageGraphAction::Enter);
	if (Node->Command)
	{
		Add(TEXT("Wrap in Sequence"), EStageGraphAction::WrapSequence);
		Add(TEXT("Wrap in Parallel"), EStageGraphAction::WrapParallel);
		Submenu(TEXT("Replace action"), EStageGraphAction::Replace);
		if (Node->Command->IsComposite()) Submenu(TEXT("Add child action"), EStageGraphAction::AddChild);
		if (Node->ReactionIndex != INDEX_NONE && Node->Command->IsComposite())
		{
			Add(TEXT("Run actions in Sequence"), EStageGraphAction::RunSequence);
			Add(TEXT("Run actions in Parallel"), EStageGraphAction::RunParallel);
		}
	}
	if (Graph->Production && Node->Kind == EStageScenarioNodeKind::Scene) Add(TEXT("Add preparation before scene"), EStageGraphAction::AddPreparation);
	if (Node->Kind == EStageScenarioNodeKind::Preparation) Add(TEXT("Remove preparation"), EStageGraphAction::RemovePreparation);
	if (Node->CanUserDeleteNode())
	{
		Add(TEXT("Copy"), EStageGraphAction::Copy);
		Add(TEXT("Duplicate"), EStageGraphAction::Duplicate);
		Add(TEXT("Delete"), EStageGraphAction::Delete);
	}
	Add(TEXT("Collapse selected nodes to subgraph"), EStageGraphAction::Collapse);
	if (Graph->bLinear && Node->CanUserDeleteNode() && !(Graph->IsStorySteps() && Graph->Scenario->bUseStepLinks))
	{
		Add(TEXT("Move earlier"), EStageGraphAction::MoveEarlier);
		Add(TEXT("Move later"), EStageGraphAction::MoveLater);
	}
}

const FPinConnectionResponse UStageScenarioGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	const auto* From = A ? Cast<UStageScenarioEdNode>(A->GetOwningNode()) : nullptr;
	const auto* To = B ? Cast<UStageScenarioEdNode>(B->GetOwningNode()) : nullptr;
	if (!From || !To || From == To || A->Direction == B->Direction || From->GetGraph() != To->GetGraph())
		return {CONNECT_RESPONSE_DISALLOW, TEXT("Choose an output and input in this graph.")};
	if (A->Direction == EGPD_Input)
	{
		Swap(From, To);
		Swap(A, B);
	}
	const auto* Graph = CastChecked<UStageScenarioEdGraph>(From->GetGraph());
	const bool bAlternate = UStageScenarioEdNode::IsAlternateExit(A->PinName);
	const bool bBranchExit = From->Kind == EStageScenarioNodeKind::Branch;
	if (Graph->IsStorySteps() && (bAlternate || bBranchExit || To->Kind == EStageScenarioNodeKind::Branch || Graph->Scenario->bUseStepLinks))
	{
		const bool bEntry = From->Kind == EStageScenarioNodeKind::Start && !Graph->ActiveGroup.IsValid();
		const bool bStep = From->bStepRoot && From->Kind != EStageScenarioNodeKind::Group;
		const bool bDraft = To->IsDraft() && Graph->Scenario->EditorDrafts.IsValidIndex(To->DraftIndex)
		    && Graph->Scenario->EditorDrafts[To->DraftIndex].Kind == EStageGraphDraftKind::Step;
		const bool bDestination = bDraft || (To->bStepRoot && To->Kind != EStageScenarioNodeKind::Group);
		if (!Graph->bReadOnly && (bStep || (bEntry && !bAlternate)) &&
		    (bDestination || (bStep && (!bAlternate || bBranchExit) && To->Kind == EStageScenarioNodeKind::Finale)))
			return {CONNECT_RESPONSE_MAKE, TEXT("Connect the Step exit to this destination.")};
		return {CONNECT_RESPONSE_DISALLOW, TEXT("Connect from Execute or a Step to a Step. Expand a group to edit its Steps.")};
	}
	if ((From->Kind == EStageScenarioNodeKind::Group && From->RepresentedIds.IsEmpty()) || (To->Kind == EStageScenarioNodeKind::Group && To->RepresentedIds.IsEmpty()))
		return {CONNECT_RESPONSE_DISALLOW, TEXT("Open this block and connect its actions first.")};
	if (From->IsDraft() || To->IsDraft())
	{
		if (Graph->bReadOnly) return {CONNECT_RESPONSE_DISALLOW, TEXT("Graph is read-only.")};
		if (From->IsDraft() && To->IsDraft())
		{
			if (Graph->Scenario && From->DraftIndex == To->DraftIndex && From->Kind == EStageScenarioNodeKind::Reaction && To->Kind == EStageScenarioNodeKind::Command)
				return {CONNECT_RESPONSE_MAKE, TEXT("Connect reaction to its action.")};
			return {CONNECT_RESPONSE_DISALLOW, TEXT("Connect from Execute or an existing execution chain first.")};
		}
		if (From->Kind == EStageScenarioNodeKind::Finale) return {CONNECT_RESPONSE_DISALLOW, TEXT("Finale remains last.")};
		if (!Graph->bLinear && !Graph->bReactions && From->Kind != EStageScenarioNodeKind::Start && To->Kind != EStageScenarioNodeKind::Return)
			return {CONNECT_RESPONSE_DISALLOW, TEXT("Connect a parallel branch from Execute or to All completed.")};
		return {CONNECT_RESPONSE_MAKE, TEXT("Connect this node to execution. Unconnected nodes do not run.")};
	}
	if (Graph->Production && (From->Kind == EStageScenarioNodeKind::Preparation || To->Kind == EStageScenarioNodeKind::Preparation))
		return {CONNECT_RESPONSE_DISALLOW, TEXT("Preparation belongs to its next Scene. Move that Scene or collapse both together.")};
	if (Graph->bReadOnly || !Graph->bLinear || To->Kind == EStageScenarioNodeKind::Return || To->Kind == EStageScenarioNodeKind::Finale
	    || From->Kind == EStageScenarioNodeKind::Finale)
		return {CONNECT_RESPONSE_DISALLOW, TEXT("Parallel branches start together and join automatically. Finale remains last.")};
	return {CONNECT_RESPONSE_MAKE, TEXT("Move this action/block immediately after the source. Execution order is updated.")};
}

bool UStageScenarioGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	if (CanCreateConnection(A, B).Response != CONNECT_RESPONSE_MAKE) return false;
	auto* From = CastChecked<UStageScenarioEdNode>(A->GetOwningNode());
	auto* To = CastChecked<UStageScenarioEdNode>(B->GetOwningNode());
	if (A->Direction == EGPD_Input)
	{
		Swap(From, To);
		Swap(A, B);
	}
	auto* Graph = CastChecked<UStageScenarioEdGraph>(From->GetGraph());
	const bool bAlternate = UStageScenarioEdNode::IsAlternateExit(A->PinName);
	if (Graph->OnConnect)
		return Graph->OnConnect(To, From, bAlternate);
	const bool bLinked = Graph->IsStorySteps() && (Graph->Scenario->bUseStepLinks || bAlternate);
	const bool Result = From->IsDraft() || To->IsDraft() ? FStageGraphDrafts::Connect(Graph, To, From, bAlternate)
	    : bLinked                                         ? FStageScenarioGraphAdapter(Graph->Scenario).ConnectStep(To, From, bAlternate)
	                                                      : Graph->Scenario && FStageScenarioGraphAdapter(Graph->Scenario).MoveElementsAfter(Graph, To, From);
	if (Result && Graph->OnEdited) Graph->OnEdited();
	return Result;
}

void UStageScenarioGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const bool bAlternate = UStageScenarioEdNode::IsAlternateExit(TargetPin.PinName);
	if ((!bAlternate && TargetPin.PinName != UStageScenarioEdNode::TruePin) || TargetPin.LinkedTo.IsEmpty()) return;
	auto* Node = CastChecked<UStageScenarioEdNode>(TargetPin.GetOwningNode());
	auto* Graph = CastChecked<UStageScenarioEdGraph>(Node->GetGraph());
	if (!Graph->bReadOnly && Graph->IsStorySteps() && Node->Kind != EStageScenarioNodeKind::Group)
		if (FStageScenarioGraphAdapter(Graph->Scenario).ConnectStep(nullptr, Node, bAlternate) && Graph->OnEdited) Graph->OnEdited();
}

void UStageScenarioGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	UEdGraphPin* Exit = SourcePin && SourcePin->Direction == EGPD_Output ? SourcePin : TargetPin;
	if (Exit) BreakPinLinks(*Exit, true);
}
