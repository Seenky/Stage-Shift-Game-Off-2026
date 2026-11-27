#include "StageGraphOrganization.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "Scenario/StageScenarioEdNode.h"
#include "StageScenarioGraphAdapter.h"
#include "StageProductionGraphAdapter.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "Stage/Core/SequenceStageMemberCommand.h"
#include "Stage/Core/ParallelStageMemberCommand.h"
#include "ScopedTransaction.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"

void FStageGraphOrganization::Project(UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Logical)
{
	TArray<UStageScenarioEdNode*> Visible;
	TMap<FGuid, UStageScenarioEdNode*> Cards;
	for (auto* Item : Logical)
	{
		Item->RepresentedIds = {Item->NodeGuid};
		const auto* Group = Graph->Groups().FindByPredicate([&](const auto& G)
		{
			return G.Scope == Graph->ScopeId && G.Members.Contains(Item->NodeGuid);
		});
		const FStageGraphGroup* Child = Group;
		TSet<FGuid> Visited;
		while (Child && Child->Parent != Graph->ActiveGroup && !Visited.Contains(Child->Id))
		{
			Visited.Add(Child->Id);
			const FGuid Parent = Child->Parent;
			Child = Graph->Groups().FindByPredicate([&](const auto& G)
			{
				return G.Id == Parent && G.Scope == Graph->ScopeId;
			});
		}
		if (Group && Group->Id == Graph->ActiveGroup) Child = nullptr;
		if (Child && Child->Id == Graph->ActiveGroup) Child = nullptr;
		if (Child && Child->Parent == Graph->ActiveGroup)
		{
			auto*& Card = Cards.FindOrAdd(Child->Id);
			if (!Card)
			{
				Card = NewObject<UStageScenarioEdNode>(Graph, NAME_None, RF_Transactional);
				Card->Kind = EStageScenarioNodeKind::Group;
				Card->GroupId = Child->Id;
				Card->NodeGuid = Child->Id;
				Card->Title = FText::FromString(Child->Title);
				Card->bCanRenameNode = true;
				Card->StepIndex = Item->StepIndex;
				Card->ParentCommand = Item->ParentCommand;
				Card->bStepRoot = Item->bStepRoot;
				Card->NodePosX = Item->NodePosX;
				Card->NodePosY = Item->NodePosY;
				Card->AllocateDefaultPins();
				Graph->AddNode(Card);
				Visible.Add(Card);
			}
			Card->RepresentedIds.Add(Item->NodeGuid);
		}
		else if ((!Group && !Graph->ActiveGroup.IsValid()) || (Group && Group->Id == Graph->ActiveGroup))
			Visible.Add(Item);
	}
	Graph->Nodes.Reset();
	auto Boundary = [&](EStageScenarioNodeKind Kind, const TCHAR* Name, int32 X)
	{
		auto* Item = NewObject<UStageScenarioEdNode>(Graph, NAME_None, RF_Transactional);
		Item->Kind = Kind;
		Item->Title = FText::FromString(Name);
		Item->NodeGuid = FGuid(0, 0, Kind == EStageScenarioNodeKind::Start ? 1 : 2, GetTypeHash(Graph->ScopeId) ^ GetTypeHash(Graph->ActiveGroup));
		Item->NodePosX = X;
		Item->NodePosY = 0;
		if (const auto* P = Graph->Positions().Find(Item->NodeGuid))
		{
			Item->NodePosX = P->X;
			Item->NodePosY = P->Y;
		}
		Item->AllocateDefaultPins();
		Graph->AddNode(Item);
		return Item;
	};
	auto* Entry = Boundary(EStageScenarioNodeKind::Start, TEXT("Execute"), -270);
	const bool bLinkedStory = Graph->IsStorySteps() && Graph->Scenario->bUseStepLinks;
	UStageScenarioEdNode* Previous = Entry;
	auto Link = [](UStageScenarioEdNode* A, UStageScenarioEdNode* B, bool bOnFailed = false)
	{
		if (!A || !B || A == B) return;
		auto* Out = A->FindPin(A->GetExitPinName(bOnFailed), EGPD_Output);
		if (!Out && bOnFailed && A->Kind == EStageScenarioNodeKind::Group)
			Out = A->CreatePin(EGPD_Output, TEXT("StageOrder"), UStageScenarioEdNode::FailedPin);
		if (!Out && !bOnFailed) Out = A->FindPin(TEXT("Execute"), EGPD_Output);
		auto* In = B->FindPin(TEXT("Execute"), EGPD_Input);
		if (Out && In && !Out->LinkedTo.Contains(In)) Out->MakeLinkTo(In);
	};
	int32 Index = 0;
	for (auto* Item : Visible)
	{
		Item->NodePosX = Graph->bLinear ? Index * 330 : Index * 330;
		Item->NodePosY = Graph->bLinear ? 0 : 180;
		if (Item->Kind == EStageScenarioNodeKind::Group) Item->Summary = FString::Printf(TEXT("%d nodes · double-click to open"), Item->RepresentedIds.Num());
		if (const auto* Position = Graph->Positions().Find(Item->NodeGuid))
		{
			Item->NodePosX = Position->X;
			Item->NodePosY = Position->Y;
		}
		Graph->Nodes.Add(Item);
		if (!bLinkedStory) Link(Graph->bLinear ? Previous : Entry, Item);
		Previous = Item;
		++Index;
	}
	if (bLinkedStory)
	{
		TMap<FGuid, UStageScenarioEdNode*> VisibleById;
		for (auto* Item : Visible)
			for (const FGuid& Id : Item->RepresentedIds) VisibleById.Add(Id, Item);
		const auto* Data = Graph->Scenario.Get();
		const auto FindStep = [&](FName Id)
		{
			return VisibleById.FindRef(FStageScenarioGraphAdapter::StepKey(Data, Data->FindStepIndex(Id)));
		};
		UStageScenarioEdNode* Exit = nullptr;
		if (Graph->ActiveGroup.IsValid())
			Exit = Boundary(EStageScenarioNodeKind::Return, TEXT("Return"), Index * 330);
		else if (!Logical.IsEmpty() && Logical.Last()->Kind == EStageScenarioNodeKind::Finale)
			Exit = Logical.Last();
		Link(Entry, FindStep(Data->EntryStepId));
		for (const FStageStep& Step : Data->Steps)
		{
			auto* From = FindStep(Step.StepId);
			auto* Then = FindStep(Step.ThenStepId);
			auto* Failed = FindStep(Step.FailedStepId);
			if (From)
			{
				Link(From, Then ? Then : Exit);
				if (Step.Branch || !Step.FailedStepId.IsNone()) Link(From, Failed ? Failed : Exit, true);
			}
			else if (Graph->ActiveGroup.IsValid())
			{
				Link(Entry, Then);
				Link(Entry, Failed);
			}
		}
	}
	else if (Graph->ActiveGroup.IsValid() || Graph->ScopeCommand || Graph->bParallelAssignments || Graph->bReactionAssignments)
	{
		const auto* Parallel = Cast<UParallelStageMemberCommand>(Graph->ScopeCommand);
		const TCHAR* ExitTitle = Graph->bLinear ? TEXT("Return") : Parallel && Parallel->bFinishOnPrimaryBranch ? TEXT("Primary completed") : TEXT("All completed");
		auto* Exit = Boundary(EStageScenarioNodeKind::Return, ExitTitle, Index * 330);
		if (Graph->bLinear)
			Link(Previous, Exit);
		else
			for (auto* Item : Visible)
				Link(Item, Exit);
	}
	Graph->NotifyGraphChanged();
}

bool FStageGraphOrganization::CanCollapse(const UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Nodes)
{
	if (!Graph || (Graph->bReactions && !Graph->ScopeCommand) || Nodes.Num() < 2) return false;
	TArray<int32> Indices;
	for (auto* Node : Nodes)
	{
		if (!Node || Node->IsDraft() || Node->GetGraph() != Graph || Node->Kind == EStageScenarioNodeKind::Start || Node->Kind == EStageScenarioNodeKind::Return
		    || Node->Kind == EStageScenarioNodeKind::Finale)
			return false;
		Indices.Add(Graph->Nodes.IndexOfByKey(Node));
	}
	Indices.Sort();
	for (int32 I = 1; I < Indices.Num(); ++I)
		if (Indices[I] != Indices[I - 1] + 1) return false;
	if (Graph->Production)
	{
		TSet<FGuid> Ids;
		for (auto* N : Nodes)
			for (FGuid Id : N->RepresentedIds)
				Ids.Add(Id);
		for (int32 I = 0; I < Graph->Production->Scenes.Num(); ++I)
			if (Graph->Production->Scenes[I].bPrepareBefore
			    && Ids.Contains(FStageProductionGraphAdapter::SceneKey(Graph->Production, I)) != Ids.Contains(FStageProductionGraphAdapter::SceneKey(Graph->Production, I, true)))
				return false;
	}
	return true;
}

bool FStageGraphOrganization::Collapse(UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Nodes, const FString& Name)
{
	if (!CanCollapse(Graph, Nodes) || !FStageScenarioGraphAdapter::Backup(Graph->GetAsset())) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Collapse", "Collapse to subgraph"));
	Graph->GetAsset()->Modify();
	if (Graph->Scenario)
		FStageScenarioGraphAdapter(Graph->Scenario).EnsureIdentities();
	else
		FStageProductionGraphAdapter(Graph->Production).EnsureIdentities();
	FStageGraphGroup New;
	New.Id = FGuid::NewGuid();
	New.Scope = Graph->ScopeId;
	New.Parent = Graph->ActiveGroup;
	New.Title = Name.IsEmpty() ? TEXT("Block") : Name;
	for (auto* Node : Nodes)
	{
		if (Node->Kind == EStageScenarioNodeKind::Group)
		{
			if (auto* G = Graph->Groups().FindByPredicate([&](const auto& Item)
			{
				return Item.Id == Node->GroupId;
			}))
				G->Parent = New.Id;
		}
		else
			New.Members.Add(Node->NodeGuid);
	}
	if (New.Parent.IsValid())
		if (auto* Parent = Graph->Groups().FindByPredicate([&](const auto& G)
		{
			return G.Id == New.Parent;
		}))
			for (FGuid Member : New.Members)
				Parent->Members.Remove(Member);
	Graph->Positions().Add(New.Id, FVector2D(Nodes[0]->NodePosX, Nodes[0]->NodePosY));
	Graph->Groups().Add(New);
	Graph->GetAsset()->MarkPackageDirty();
	return true;
}

bool FStageGraphOrganization::Expand(UStageScenarioEdGraph* Graph, FGuid Id)
{
	const auto* Found = Graph->Groups().FindByPredicate([&](const auto& G)
	{
		return G.Id == Id;
	});
	if (!Found || !FStageScenarioGraphAdapter::Backup(Graph->GetAsset())) return false;
	const FStageGraphGroup Copy = *Found;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Expand", "Expand subgraph"));
	Graph->GetAsset()->Modify();
	for (auto& G : Graph->Groups())
		if (G.Parent == Id) G.Parent = Copy.Parent;
	if (Copy.Parent.IsValid())
		if (auto* Parent = Graph->Groups().FindByPredicate([&](const auto& G)
		{
			return G.Id == Copy.Parent;
		}))
			Parent->Members.Append(Copy.Members);
	Graph->Groups().RemoveAll([&](const auto& G)
	{
		return G.Id == Id;
	});
	Graph->Positions().Remove(Id);
	Graph->GetAsset()->MarkPackageDirty();
	return true;
}

bool FStageGraphOrganization::Rename(UStageScenarioEdGraph* Graph, UStageScenarioEdNode* Node, const FString& Name)
{
	if (!Node || Name.TrimStartAndEnd().IsEmpty() || !FStageScenarioGraphAdapter::Backup(Graph->GetAsset())) return false;
	const FName Id(*Name.TrimStartAndEnd());
	if (Graph->Production && Node->Kind == EStageScenarioNodeKind::Scene
	    && Graph->Production->Scenes.ContainsByPredicate([&](const auto& S)
	{
		return &S != &Graph->Production->Scenes[Node->StepIndex] && S.SceneId == Id;
	}))
		return false;
	if (Graph->Scenario && Node->bStepRoot && Node->Kind != EStageScenarioNodeKind::Group
	    && Graph->Scenario->Steps.ContainsByPredicate([&](const auto& S)
	{
		return &S != &Graph->Scenario->Steps[Node->StepIndex] && S.StepId == Id;
	}))
		return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Rename", "Rename scenario node"));
	Graph->GetAsset()->Modify();
	if (Node->Kind == EStageScenarioNodeKind::Group)
	{
		if (auto* G = Graph->Groups().FindByPredicate([&](const auto& Item)
		{
			return Item.Id == Node->GroupId;
		}))
			G->Title = Name;
	}
	else if (Graph->Production && Node->Kind == EStageScenarioNodeKind::Scene && Graph->Production->Scenes.IsValidIndex(Node->StepIndex))
		Graph->Production->Scenes[Node->StepIndex].SceneId = Id;
	else if (Graph->Scenario && Node->bStepRoot && Graph->Scenario->Steps.IsValidIndex(Node->StepIndex))
	{
		auto* Data = Graph->Scenario.Get();
		const FName OldId = Data->Steps[Node->StepIndex].StepId;
		Data->Steps[Node->StepIndex].StepId = Id;
		if (!OldId.IsNone())
		{
			const auto RenameReactionStep = [OldId, Id](FStageMemberReaction& Reaction)
			{
				if (Reaction.AllowedFromStepId == OldId) Reaction.AllowedFromStepId = Id;
				if (Reaction.AllowedToStepId == OldId) Reaction.AllowedToStepId = Id;
				for (FName& Allowed : Reaction.AllowedStepIds)
					if (Allowed == OldId) Allowed = Id;
			};
			for (FStageMemberReaction& Reaction : Data->MemberReactions) RenameReactionStep(Reaction);
			for (FStageGraphDraft& Draft : Data->EditorDrafts)
				if (Draft.Kind == EStageGraphDraftKind::Reaction) RenameReactionStep(Draft.Reaction);
		}
		if (Data->bUseStepLinks)
		{
			if (Data->EntryStepId == OldId) Data->EntryStepId = Id;
			for (FStageStep& Step : Data->Steps)
			{
				if (Step.ThenStepId == OldId) Step.ThenStepId = Id;
				if (Step.FailedStepId == OldId) Step.FailedStepId = Id;
			}
		}
	}
	else
		return false;
	Graph->GetAsset()->MarkPackageDirty();
	return true;
}

bool FStageGraphOrganization::Layout(UStageScenarioEdGraph* Graph, const SGraphPanel* Panel)
{
	if (!Graph || Graph->bReadOnly || Graph->Nodes.IsEmpty()) return false;

	TMap<const UEdGraphNode*, FIntPoint> Cells;
	TArray<TPair<const UEdGraphNode*, FIntPoint>> Branches;
	TArray<int32> NextRowByColumn;
	int32 NextSectionRow = 0;
	const auto PlaceChain = [&](const UEdGraphNode* Node, FIntPoint Cell, bool bVertical)
	{
		while (Node && !Cells.Contains(Node))
		{
			Cells.Add(Node, Cell);
			if (!NextRowByColumn.IsValidIndex(Cell.X)) NextRowByColumn.SetNumZeroed(Cell.X + 1);
			NextRowByColumn[Cell.X] = FMath::Max(NextRowByColumn[Cell.X], Cell.Y + 1);
			NextSectionRow = FMath::Max(NextSectionRow, Cell.Y + 1);

			const UEdGraphNode* Next = nullptr;
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction != EGPD_Output) continue;
				const bool bAlternate = UStageScenarioEdNode::IsAlternateExit(Pin->PinName);
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					const UEdGraphNode* Target = Linked->GetOwningNode();
					if (bAlternate || Next) Branches.Emplace(Target, FIntPoint(Cell.X + (bAlternate ? 0 : 1), Cell.Y + 1));
					else Next = Target;
				}
			}
			Node = Next;
			if (bVertical) ++Cell.Y;
			else ++Cell.X;
		}
	};
	const auto PlaceSection = [&](const UEdGraphNode* Root)
	{
		PlaceChain(Root, FIntPoint(0, NextSectionRow), false);
		// Lay out the main chain first so branches can join it without moving shared nodes.
		for (int32 Index = 0; Index < Branches.Num(); ++Index)
		{
			const auto [Target, Start] = Branches[Index];
			const int32 FreeRow = NextRowByColumn.IsValidIndex(Start.X) ? NextRowByColumn[Start.X] : 0;
			PlaceChain(Target, FIntPoint(Start.X, FMath::Max(Start.Y, FreeRow)), true);
		}
		Branches.Reset();
	};
	for (const UEdGraphNode* Node : Graph->Nodes)
		if (!Node->Pins.ContainsByPredicate([](const UEdGraphPin* Pin) { return Pin->Direction == EGPD_Input && !Pin->LinkedTo.IsEmpty(); }))
			PlaceSection(Node);
	for (const UEdGraphNode* Node : Graph->Nodes)
		if (!Cells.Contains(Node)) PlaceSection(Node);

	TArray<int32> ColumnWidths, RowHeights;
	ColumnWidths.Init(330, NextRowByColumn.Num());
	RowHeights.Init(150, NextSectionRow);
	if (Panel)
		for (const auto& [Node, Cell] : Cells)
			if (const auto Widget = Panel->GetNodeWidgetFromGuid(Node->NodeGuid))
			{
				const FVector2D Size = Widget->GetDesiredSize();
				ColumnWidths[Cell.X] = FMath::Max(ColumnWidths[Cell.X], FMath::CeilToInt(Size.X) + 96);
				RowHeights[Cell.Y] = FMath::Max(RowHeights[Cell.Y], FMath::CeilToInt(Size.Y) + 56);
			}
	const auto Offsets = [](TArrayView<const int32> Sizes, int32 Start)
	{
		TArray<int32> Result;
		Result.Reserve(Sizes.Num());
		for (const int32 Size : Sizes)
		{
			Result.Add(Start);
			Start += Size;
		}
		return Result;
	};
	const auto Columns = Offsets(ColumnWidths, -330);
	const auto Rows = Offsets(RowHeights, 0);
	auto& Current = Graph->Positions();
	TMap<FGuid, FVector2D> Changes;
	for (const auto& [Node, Cell] : Cells)
	{
		const FVector2D Position(Columns[Cell.X], Rows[Cell.Y]);
		const auto* Previous = Current.Find(Node->NodeGuid);
		if (!Previous || *Previous != Position) Changes.Add(Node->NodeGuid, Position);
	}
	if (Changes.IsEmpty() || !FStageScenarioGraphAdapter::Backup(Graph->GetAsset())) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Layout", "Layout visible graph"));
	Graph->GetAsset()->Modify();
	Current.Append(MoveTemp(Changes));
	Graph->GetAsset()->MarkPackageDirty();
	return true;
}

bool FStageGraphOrganization::Delete(UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Nodes)
{
	TSet<FGuid> Ids;
	TSet<int32> Reactions;
	TMap<int32, TSet<FGuid>> ReactionCommands;
	for (auto* N : Nodes)
		if (N && N->CanUserDeleteNode())
		{
			for (FGuid Id : N->RepresentedIds)
				Ids.Add(Id);
			if (N->ReactionIndex != INDEX_NONE)
			{
				if (N->Kind == EStageScenarioNodeKind::Reaction)
					Reactions.Add(N->ReactionIndex);
				else
					for (FGuid Id : N->RepresentedIds)
						ReactionCommands.FindOrAdd(N->ReactionIndex).Add(Id);
			}
		}
	if (Ids.IsEmpty() && Reactions.IsEmpty()) return false;
	if (!FStageScenarioGraphAdapter::Backup(Graph->GetAsset())) return false;
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "DeleteSelection", "Delete scenario selection"));
	Graph->GetAsset()->Modify();
	if (Graph->Production)
	{
		FStageProductionGraphAdapter(Graph->Production).EnsureIdentities();
		Graph->Production->Scenes.RemoveAll([&](auto& S)
		{
			if (Ids.Contains(S.PreparationGuid)) S.bPrepareBefore = false;
			return Ids.Contains(S.EditorGuid);
		});
	}
	else
	{
		auto* Data = Graph->Scenario.Get();
		FStageScenarioGraphAdapter(Data).EnsureIdentities();
		auto BeforeSteps = Data->Steps;
		const FName BeforeEntry = Data->EntryStepId;
		auto BeforeFinale = Data->Finale;
		auto BeforeReactions = Data->MemberReactions;
		TArray<FString> BeforeErrors;
		Data->ValidateDefinition(BeforeErrors);
		TArray<TObjectPtr<UStageMemberCommand>>* Children = nullptr;
		if (auto* S = Cast<USequenceStageMemberCommand>(Graph->ScopeCommand)) Children = &S->Children;
		if (auto* P = Cast<UParallelStageMemberCommand>(Graph->ScopeCommand)) Children = &P->Children;
		TArray<TObjectPtr<UStageMemberCommand>> BeforeChildren;
		if (Children)
		{
			Graph->ScopeCommand->Modify();
			BeforeChildren = *Children;
			Children->RemoveAll([&](const auto& C)
			{
				return Ids.Contains(FStageScenarioGraphAdapter::Key(C));
			});
		}
		else if (Graph->bReactions)
		{
			for (int32 I = Data->MemberReactions.Num() - 1; I >= 0; --I)
			{
				if (Reactions.Contains(I))
				{
					Data->MemberReactions.RemoveAt(I);
					continue;
				}
				const auto* RemovedIds = ReactionCommands.Find(I);
				if (!RemovedIds) continue;
				auto& R = Data->MemberReactions[I];
				R.AdditionalPerformers.RemoveAll([&](const auto& A)
				{
					return RemovedIds->Contains(FStageScenarioGraphAdapter::Key(A.Command));
				});
				if (RemovedIds->Contains(FStageScenarioGraphAdapter::Key(R.Command)))
				{
					if (R.AdditionalPerformers.IsEmpty())
						Data->MemberReactions.RemoveAt(I);
					else
					{
						R.RoleId = R.AdditionalPerformers[0].GetEffectiveRoleId();
						R.MemberTag = {};
						R.Command = R.AdditionalPerformers[0].Command;
						R.AdditionalPerformers.RemoveAt(0);
					}
				}
			}
		}
		else if (Graph->bParallelAssignments)
		{
			auto& Entries = Graph->ScopeStep == INDEX_NONE ? Data->Finale : Data->Steps[Graph->ScopeStep].Commands;
			Entries.RemoveAll([&](const auto& A)
			{
				return Ids.Contains(FStageScenarioGraphAdapter::Key(A.Command));
			});
		}
		else
		{
			Data->Steps.RemoveAll([&](const auto& S)
			{
				return Ids.Contains(S.EditorGuid);
			});
			if (Data->bUseStepLinks)
			{
				const auto AfterDeleted = [&](FName Id) -> FName
				{
					for (int32 Hop = 0; !Id.IsNone() && Data->FindStepIndex(Id) == INDEX_NONE; ++Hop)
					{
						const FStageStep* Removed = BeforeSteps.FindByPredicate([Id](const auto& S) { return S.StepId == Id; });
						if (!Removed || Hop >= BeforeSteps.Num()) return NAME_None;
						Id = Removed->ThenStepId;
					}
					return Id;
				};
				Data->EntryStepId = AfterDeleted(Data->EntryStepId);
				for (auto& Step : Data->Steps)
				{
					Step.ThenStepId = AfterDeleted(Step.ThenStepId);
					Step.FailedStepId = AfterDeleted(Step.FailedStepId);
				}
			}
		}
		TArray<FString> Errors;
		Data->ValidateDefinition(Errors);
		if (Errors.ContainsByPredicate([&](const FString& E)
		{
			return E.Contains(TEXT("RequiredSuccessRoleId")) && !BeforeErrors.Contains(E);
		}))
		{
			Data->Steps = BeforeSteps;
			Data->EntryStepId = BeforeEntry;
			Data->Finale = BeforeFinale;
			Data->MemberReactions = BeforeReactions;
			if (Children) *Children = BeforeChildren;
			Transaction.Cancel();
			return false;
		}
		if (auto* Parallel = Cast<UParallelStageMemberCommand>(Graph->ScopeCommand); Parallel && !Parallel->Children.Contains(Parallel->PrimaryBranch))
			Parallel->PrimaryBranch = nullptr;
	}
	for (auto& G : Graph->Groups())
		G.Members.RemoveAll([&](FGuid Id)
		{
			return Ids.Contains(Id);
		});
	bool Removed = true;
	while (Removed)
	{
		Removed = false;
		for (int32 I = Graph->Groups().Num() - 1; I >= 0; --I)
		{
			const FGuid Id = Graph->Groups()[I].Id;
			if (Graph->Groups()[I].Members.IsEmpty()
			    && !Graph->Groups().ContainsByPredicate(
			        [&](const auto& G)
			{
				return G.Parent == Id;
			})
			    && !(Graph->Scenario
			        && Graph->Scenario->EditorDrafts.ContainsByPredicate(
			            [&](const auto& D)
			{
				return D.Group == Id;
			}))
			    && !(Graph->Production
			        && Graph->Production->EditorDrafts.ContainsByPredicate([&](const auto& D)
			{
				return D.Group == Id;
			})))
			{
				Graph->Groups().RemoveAt(I);
				Removed = true;
			}
		}
	}
	Graph->GetAsset()->MarkPackageDirty();
	Graph->GetAsset()->PostEditChange();
	return true;
}
