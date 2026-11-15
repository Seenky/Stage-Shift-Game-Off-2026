#include "StageScenarioEditorToolkit.h"
#include "StageScenarioGraphAdapter.h"
#include "StageProductionGraphAdapter.h"
#include "StageGraphOrganization.h"
#include "StageGraphClipboard.h"
#include "StageGraphDrafts.h"
#include "StageGraphParameters.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "Stage/Core/StageMemberCommand.h"
#include "Stage/Core/SequenceStageMemberCommand.h"
#include "Stage/Core/ParallelStageMemberCommand.h"
#include "Editor.h"
#include "GraphEditor.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Commands/GenericCommands.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Containers/Ticker.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const FName StoryTab(TEXT("StageStory")), ReactionTab(TEXT("StageReactions")), SettingsTab(TEXT("StageSettings")), DetailsTab(TEXT("StageDetails"));
}

FText FStageScenarioEditorToolkit::GetBaseToolkitName() const
{
	return FText::FromString(Production ? TEXT("Stage Production") : TEXT("Stage Scenario"));
}

void FStageScenarioEditorToolkit::Init(UObject* InAsset, EToolkitMode::Type Mode, TSharedPtr<IToolkitHost> Host)
{
	Asset = InAsset;
	Data = Cast<UStageDataAsset>(Asset);
	Production = Cast<UStageProductionDataAsset>(Asset);
	check(Data || Production);
	Asset->SetFlags(RF_Transactional);
	bEditable = FStageScenarioGraphAdapter::Backup(Asset);
	for (TObjectIterator<UStageMemberCommand> It; It; ++It)
		if (It->IsIn(Asset)) It->SetFlags(RF_Transactional);
	auto& Properties = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs Args;
	Args.bAllowSearch = true;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = this;
	Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	CommandDetails = Properties.CreateDetailView(Args);
	AssetDetails = Properties.CreateDetailView(Args);
	CommandDetails->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P)
	{
		return P.Property.GetFName() != TEXT("Children") && P.Property.GetFName() != TEXT("GraphParameterBindings");
	}));
	CommandDetails->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateLambda([this](const FPropertyAndParent& P)
	{
		return FStageGraphParameters::IsBound(Selected.IsValid() ? Selected->Command.Get() : nullptr, P);
	}));
	AssetDetails->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P)
	{
		const FName N = P.Property.GetFName();
		return N != TEXT("Steps") && N != TEXT("MemberReactions") && N != TEXT("Finale") && N != TEXT("Scenes");
	}));
	AssetDetails->SetObject(Asset);
	ItemDetails = Properties.CreateStructureDetailView(Args, FStructureDetailsViewArgs(), nullptr);
	ItemDetails->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P)
	{
		const FName N = P.Property.GetFName();
		if (P.ParentProperties.ContainsByPredicate([](const FProperty* Parent)
		{
			return Parent->GetFName() == TEXT("Command") || Parent->GetFName() == TEXT("Commands");
		}))
			return false;
		return N != TEXT("Command") && N != TEXT("Commands") && N != TEXT("AdditionalPerformers") && N != TEXT("RoleId") && N != TEXT("SubjectRoleId");
	}));
	for (IDetailsView* Details : {CommandDetails.Get(), AssetDetails.Get(), ItemDetails->GetDetailsView()})
		Details->SetIsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateLambda([this]
		{
			return bEditable;
		}));
	GEditor->RegisterForUndo(this);
	PropertyHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FStageScenarioEditorToolkit::Changed);
	auto Center = FTabManager::NewStack()->AddTab(StoryTab, ETabState::OpenedTab);
	if (Data) Center->AddTab(ReactionTab, ETabState::OpenedTab);
	Center->SetForegroundTab(StoryTab);
	auto Layout = FTabManager::NewLayout(TEXT("StageScenarioEditor_v3"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.18f)
				->AddTab(SettingsTab, ETabState::OpenedTab)
			)
			->Split(Center->SetSizeCoefficient(0.57f))
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->AddTab(DetailsTab, ETabState::OpenedTab)
			)
		);
	InitAssetEditor(Mode, Host, TEXT("StageScenarioEditor"), Layout, true, true, Asset);
	auto Bind = [&](const TSharedPtr<FUICommandInfo>& Command, EStageGraphAction Op)
	{
		GetToolkitCommands()->MapAction(Command,
		    FExecuteAction::CreateLambda([this, Op]
		{
			if (ActiveGraph) Action(ActiveGraph, Op, nullptr, Selected.Get(), FVector2D(View(ActiveGraph)->GetPasteLocation2f()));
		}));
	};
	Bind(FGenericCommands::Get().Delete, EStageGraphAction::Delete);
	Bind(FGenericCommands::Get().Duplicate, EStageGraphAction::Duplicate);
	Bind(FGenericCommands::Get().Copy, EStageGraphAction::Copy);
	Bind(FGenericCommands::Get().Paste, EStageGraphAction::Paste);
	Bind(FGenericCommands::Get().Rename, EStageGraphAction::Rename);
	GetToolkitCommands()->MapAction(FGenericCommands::Get().Undo,
	    FExecuteAction::CreateLambda([]
	{
		GEditor->UndoTransaction();
	}));
	GetToolkitCommands()->MapAction(FGenericCommands::Get().Redo,
	    FExecuteAction::CreateLambda([]
	{
		GEditor->RedoTransaction();
	}));
	// Tabs already built their views. Rebuilding here discards their deferred initial framing.
	Validate();
}

FStageScenarioEditorToolkit::~FStageScenarioEditorToolkit()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyHandle);
	if (GEditor) GEditor->UnregisterForUndo(this);
}

void FStageScenarioEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& M)
{
	FAssetEditorToolkit::RegisterTabSpawners(M);
	auto Add = [&](FName Id, const TCHAR* Label)
	{
		M->RegisterTabSpawner(Id, FOnSpawnTab::CreateSP(this, &FStageScenarioEditorToolkit::SpawnTab)).SetDisplayName(FText::FromString(Label));
	};
	Add(StoryTab, Production ? TEXT("Production") : TEXT("Story"));
	if (Data) Add(ReactionTab, TEXT("Reactions"));
	Add(SettingsTab, TEXT("Scenario settings"));
	Add(DetailsTab, TEXT("Details"));
}

void FStageScenarioEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& M)
{
	for (FName Id : {StoryTab, ReactionTab, SettingsTab, DetailsTab})
		M->UnregisterTabSpawner(Id);
	FAssetEditorToolkit::UnregisterTabSpawners(M);
}

TSharedRef<SDockTab> FStageScenarioEditorToolkit::SpawnTab(const FSpawnTabArgs& Args)
{
	auto Tab = SNew(SDockTab);
	const FName Id = Args.GetTabId().TabType;
	if (Id == SettingsTab)
		Tab->SetContent(AssetDetails.ToSharedRef());
	else if (Id == DetailsTab)
		Tab->SetContent
		(
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.4f)
			[
				ItemDetails->GetWidget().ToSharedRef()
			]
			+ SSplitter::Slot()
			.Value(0.6f)
			[
				CommandDetails.ToSharedRef()
			]
		);
	else
	{
		const bool Reaction = Id == ReactionTab;
		Tab->SetContent
		(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				Reaction ? SAssignNew(ReactionHost, SBox) : SAssignNew(StoryHost, SBox)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.MaxDesiredHeight(85)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(STextBlock)
						.Text_Lambda([this]
						{
							return FText::FromString(Message);
						})
						.AutoWrapText(true)
					]
				]
			]
		);
		Build(Reaction);
	}
	return Tab;
}

TSharedPtr<SGraphEditor> FStageScenarioEditorToolkit::View(UStageScenarioEdGraph* Graph) const
{
	return Graph && Graph->bReactions ? ReactionView : StoryView;
}

TArray<UStageScenarioEdNode*> FStageScenarioEditorToolkit::Selection(UStageScenarioEdGraph* Graph, UStageScenarioEdNode* Fallback) const
{
	TArray<UStageScenarioEdNode*> Result;
	if (auto V = View(Graph))
		for (UObject* O : V->GetSelectedNodes())
			if (auto* N = Cast<UStageScenarioEdNode>(O)) Result.Add(N);
	if (Fallback && Fallback->GetGraph() == Graph && !Result.Contains(Fallback))
	{
		Result.Reset();
		Result.Add(Fallback);
	}
	Result.Sort([](const auto& A, const auto& B)
	{
		return A.GetGraph()->Nodes.IndexOfByKey(&A) < B.GetGraph()->Nodes.IndexOfByKey(&B);
	});
	return Result;
}

void FStageScenarioEditorToolkit::Build(bool bReaction, bool bFrame)
{
	auto Host = bReaction ? ReactionHost : StoryHost;
	if (!Host) return;
	ItemDetails->SetStructureData(nullptr);
	CommandDetails->SetObject(nullptr);
	Selected.Reset();
	auto& Stack = bReaction ? ReactionScopes : StoryScopes;
	auto& Graph = bReaction ? Reactions : Story;
	auto& Editor = bReaction ? ReactionView : StoryView;
	const bool FirstView = !Editor;
	FVector2f Location(-350, -120);
	float Zoom = 0.8f;
	if (Editor && Graph && bEditable)
	{
		Editor->GetViewLocation(Location, Zoom);
		FStageGraphViewState State;
		State.Location = FVector2D(Location);
		State.Zoom = Zoom;
		if (!Graph->Views().Contains(Graph->ViewKey()) || Graph->Views()[Graph->ViewKey()].Location != State.Location || Graph->Views()[Graph->ViewKey()].Zoom != State.Zoom)
		{
			Asset->Modify();
			Graph->Views().Add(Graph->ViewKey(), State);
			Asset->MarkPackageDirty();
		}
	}
	Graph.Reset(NewObject<UStageScenarioEdGraph>());
	Graph->Scenario = Data;
	Graph->Production = Production;
	Graph->bReadOnly = !bEditable;
	Graph->bReactions = bReaction;
	while (!Stack.IsEmpty())
	{
		const auto& S = Stack.Last();
		Graph->ActiveGroup = S.Group;
		Graph->ScopeCommand = S.Command.Get();
		Graph->bParallelAssignments = S.bAssignments;
		Graph->bReactionAssignments = S.bReactionAssignments;
		Graph->ScopeStep = INDEX_NONE;
		Graph->ScopeReaction = INDEX_NONE;
		bool Valid = true;
		if (S.bAssignments && S.Step.IsValid())
		{
			for (int32 I = 0; I < Data->Steps.Num(); ++I)
				if (FStageScenarioGraphAdapter::StepKey(Data, I) == S.Step)
				{
					Graph->ScopeStep = I;
					break;
				}
			if (Graph->ScopeStep == INDEX_NONE) Valid = false;
		}
		if (S.Group.IsValid()
		    && !Graph->Groups().ContainsByPredicate([&](const auto& G)
		{
			return G.Id == S.Group;
		}))
			Valid = false;
		if (S.bReactionAssignments)
		{
			for (int32 I = 0; I < Data->MemberReactions.Num(); ++I)
				if (FStageScenarioGraphAdapter::ReactionKey(Data, I) == S.Reaction)
				{
					Graph->ScopeReaction = I;
					break;
				}
			if (Graph->ScopeReaction == INDEX_NONE) Valid = false;
		}
		if (S.Command.IsStale()) Valid = false;
		if (S.Command.IsValid())
		{
			TSet<const UStageMemberCommand*> Seen;
			TFunction<bool(const UStageMemberCommand*)> Contains = [&](const UStageMemberCommand* C)
			{
				if (!C || Seen.Contains(C)) return false;
				if (C == S.Command.Get()) return true;
				Seen.Add(C);
				for (const UStageMemberCommand* Child : C->GetChildCommands())
					if (Contains(Child)) return true;
				return false;
			};
			bool Found = false;
			for (const auto& Step : Data->Steps)
				for (const auto& A : Step.Commands)
					Found |= Contains(A.Command);
			for (const auto& A : Data->Finale)
				Found |= Contains(A.Command);
			for (const auto& R : Data->MemberReactions)
				for (const auto& A : R.GetAssignments())
					Found |= Contains(A.Command);
			for (const auto& D : Data->EditorDrafts)
				Found |= Contains(D.GetCommand());
			Valid &= Found;
		}
		if (Valid) break;
		Stack.Pop();
		Graph->ActiveGroup.Invalidate();
		Graph->ScopeCommand = nullptr;
		Graph->bParallelAssignments = false;
		Graph->bReactionAssignments = false;
	}
	if (Production)
		FStageProductionGraphAdapter(Production).Build(Graph.Get());
	else
		FStageScenarioGraphAdapter(Data).BuildView(Graph.Get());
	auto* Raw = Graph.Get();
	Graph->OnEdited = [this]
	{
		QueueRefresh();
	};
	Graph->OnAction = [this, Raw](EStageGraphAction Op, UClass* Class, UStageScenarioEdNode* Node, FVector2D At)
	{
		Action(Raw, Op, Class, Node, At);
	};
	Graph->OnConnect = [this, Raw](UStageScenarioEdNode* Node, UStageScenarioEdNode* After)
	{
		ItemDetails->SetStructureData(nullptr);
		CommandDetails->SetObject(nullptr);
		const bool Done = Node->IsDraft() || After->IsDraft() ? FStageGraphDrafts::Connect(Raw, Node, After)
		    : Production                                      ? FStageProductionGraphAdapter(Production).MoveAfter(Raw, Node, After)
		                                                      : FStageScenarioGraphAdapter(Data).MoveElementsAfter(Raw, Node, After);
		if (!Done) Message = TEXT("Cannot connect: check role/resource conflicts and connect from Execute or an existing action.");
		QueueRefresh();
	};
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FStageScenarioEditorToolkit::Select);
	Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FStageScenarioEditorToolkit::Enter);
	Events.OnFocused = SGraphEditor::FOnFocused::CreateLambda([this, Raw](const TSharedRef<SGraphEditor>&)
	{
		ActiveGraph = Raw;
	});
	Events.OnTextCommitted = FOnNodeTextCommitted::CreateLambda([](const FText& Text, ETextCommit::Type Commit, UEdGraphNode* N)
	{
		if (Commit != ETextCommit::OnCleared) N->OnRenameNode(Text.ToString());
	});
	auto Crumbs = SNew(SHorizontalBox);
	auto Crumb = [&](FString Label, int32 Depth)
	{
		Crumbs->AddSlot()
		.AutoWidth()
		.Padding(2)
		[
			SNew(SButton)
			.Text(FText::FromString(Label))
			.OnClicked_Lambda([this, bReaction, Depth]
			{
				auto& Scopes = bReaction ? ReactionScopes : StoryScopes;
				Scopes.SetNum(Depth);
				Build(bReaction, true);
				return FReply::Handled();
			})
		];
	};
	Crumb(bReaction ? TEXT("Reactions") : Production ? TEXT("Production") : TEXT("Story"), 0);
	for (int32 I = 0; I < Stack.Num(); ++I)
		Crumb(TEXT("› ") + Stack[I].Name, I + 1);
	Host->SetContent
	(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Crumbs
		]
		+ SVerticalBox::Slot()
		.FillHeight(1)
		[
			SAssignNew(Editor, SGraphEditor)
			.GraphToEdit(Graph.Get())
			.GraphEvents(Events)
			.AdditionalCommands(GetToolkitCommands())
			.IsEditable(bEditable)
		]
	);
	if (const auto* State = Graph->Views().Find(Graph->ViewKey()))
		Editor->SetViewLocation(FVector2f(State->Location), State->Zoom);
	else if (FirstView || bFrame)
		Editor->ZoomToFit(false);
	else
		Editor->SetViewLocation(Location, Zoom);
	ActiveGraph = Raw;
}

void FStageScenarioEditorToolkit::Refresh()
{
	const bool WasReactions = ActiveGraph && ActiveGraph->bReactions;
	const FGuid Id = Selected.IsValid() ? Selected->NodeGuid : FGuid();
	const bool Reaction = Selected.IsValid() && CastChecked<UStageScenarioEdGraph>(Selected->GetGraph())->bReactions;
	ItemDetails->SetStructureData(nullptr);
	CommandDetails->SetObject(nullptr);
	Selected.Reset();
	Build(false);
	if (Data) Build(true);
	auto* Graph = Reaction ? Reactions.Get() : Story.Get();
	if (Graph)
		for (UEdGraphNode* N : Graph->Nodes)
			if (N->NodeGuid == Id)
			{
				View(Graph)->SetNodeSelection(N, true);
				break;
			}
	ActiveGraph = WasReactions ? Reactions.Get() : Story.Get();
	Validate();
}

void FStageScenarioEditorToolkit::Select(const TSet<UObject*>& Objects)
{
	ItemDetails->SetStructureData(nullptr);
	CommandDetails->SetObject(nullptr);
	Selected.Reset();
	if (Objects.Num() != 1) return;
	auto* Node = Cast<UStageScenarioEdNode>(*Objects.CreateConstIterator());
	if (!Node) return;
	Selected = Node;
	ActiveGraph = CastChecked<UStageScenarioEdGraph>(Node->GetGraph());
	CommandDetails->SetObject(Node->Command);
	if (Node->Kind == EStageScenarioNodeKind::Group) return;
	TSharedPtr<FStructOnScope> Scope;
	if (Node->IsDraft() && Production && Production->EditorDrafts.IsValidIndex(Node->DraftIndex))
		Scope = MakeShared<FStructOnScope>(FStageProductionScene::StaticStruct(), reinterpret_cast<uint8*>(&Production->EditorDrafts[Node->DraftIndex].Scene));
	else if (Node->IsDraft() && Data && Data->EditorDrafts.IsValidIndex(Node->DraftIndex))
	{
		auto& D = Data->EditorDrafts[Node->DraftIndex];
		Scope = D.Kind == EStageGraphDraftKind::Reaction ? MakeShared<FStructOnScope>(FStageMemberReaction::StaticStruct(), reinterpret_cast<uint8*>(&D.Reaction))
		                                                 : MakeShared<FStructOnScope>(FStageStep::StaticStruct(), reinterpret_cast<uint8*>(&D.Step));
	}
	else if (Production && Production->Scenes.IsValidIndex(Node->StepIndex))
		Scope = MakeShared<FStructOnScope>(FStageProductionScene::StaticStruct(), reinterpret_cast<uint8*>(&Production->Scenes[Node->StepIndex]));
	else if (Data && Data->MemberReactions.IsValidIndex(Node->ReactionIndex))
		Scope = MakeShared<FStructOnScope>(FStageMemberReaction::StaticStruct(), reinterpret_cast<uint8*>(&Data->MemberReactions[Node->ReactionIndex]));
	else if (Data && Node->bStepRoot && Data->Steps.IsValidIndex(Node->StepIndex))
		Scope = MakeShared<FStructOnScope>(FStageStep::StaticStruct(), reinterpret_cast<uint8*>(&Data->Steps[Node->StepIndex]));
	if (Scope) Scope->SetPackage(Asset->GetOutermost());
	ItemDetails->SetStructureData(Scope);
}

void FStageScenarioEditorToolkit::Enter(UEdGraphNode* Base)
{
	auto* Node = Cast<UStageScenarioEdNode>(Base);
	if (!Node) return;
	auto* Graph = CastChecked<UStageScenarioEdGraph>(Node->GetGraph());
	if (Production && Node->Kind == EStageScenarioNodeKind::Scene)
	{
		auto* Scenario = Node->IsDraft() ? Production->EditorDrafts[Node->DraftIndex].Scene.Scenario.Get() : Production->Scenes[Node->StepIndex].Scenario.Get();
		if (Scenario) GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Scenario);
		return;
	}
	auto& Stack = Graph->bReactions ? ReactionScopes : StoryScopes;
	FStageViewScope S;
	if (!Stack.IsEmpty()) S = Stack.Last();
	S.Name = Node->GetNodeTitle(ENodeTitleType::EditableTitle).ToString();
	if (Node->Kind == EStageScenarioNodeKind::Group)
		S.Group = Node->GroupId;
	else if (Node->Kind == EStageScenarioNodeKind::Reaction && !Node->IsDraft())
	{
		S.Command.Reset();
		S.Group.Invalidate();
		S.bAssignments = false;
		S.bReactionAssignments = true;
		S.Reaction = FStageScenarioGraphAdapter::ReactionKey(Data, Node->ReactionIndex);
		S.Name = Data->MemberReactions[Node->ReactionIndex].ReactionId.ToString();
	}
	else if (Node->Command && Node->Command->IsComposite())
	{
		S.Command = Node->Command;
		S.bAssignments = false;
		S.bReactionAssignments = false;
		S.Group.Invalidate();
	}
	else if (Node->Kind == EStageScenarioNodeKind::Step || Node->Kind == EStageScenarioNodeKind::Finale)
	{
		S.Command.Reset();
		S.Group.Invalidate();
		S.bAssignments = true;
		S.Step = Node->Kind == EStageScenarioNodeKind::Step ? FStageScenarioGraphAdapter::StepKey(Data, Node->StepIndex) : FGuid();
	}
	else
		return;
	Stack.Add(S);
	Build(Graph->bReactions, true);
}

void FStageScenarioEditorToolkit::Action(UStageScenarioEdGraph* Graph, EStageGraphAction Op, UClass* Class, UStageScenarioEdNode* Node, FVector2D Location)
{
	if (!bEditable || !Graph) return;
	if (Node && Node->GetGraph() != Graph) Node = nullptr;
	ActiveGraph = Graph;
	auto Nodes = Selection(Graph, Node);
	if (!Node && !Nodes.IsEmpty()) Node = Nodes[0];
	if (Op == EStageGraphAction::Validate)
	{
		Validate();
		return;
	}
	if (Op == EStageGraphAction::Fit)
	{
		View(Graph)->ZoomToFit(false);
		return;
	}
	if (Op == EStageGraphAction::Enter)
	{
		Enter(Node);
		return;
	}
	if (Op == EStageGraphAction::Rename)
	{
		if (Node) View(Graph)->JumpToNode(Node, true);
		return;
	}
	if ((Op == EStageGraphAction::AddChild || Op == EStageGraphAction::AddParallelRole) && Node)
	{
		const bool ReactionsView = Graph->bReactions;
		if (Op == EStageGraphAction::AddChild)
		{
			if (Node->Kind == EStageScenarioNodeKind::Reaction && !Node->IsDraft())
			{
				auto* Context = NewObject<UStageScenarioEdNode>(Graph);
				Context->ReactionIndex = Node->ReactionIndex;
				Context->AssignmentIndex = 0;
				Context->Command = Data->MemberReactions[Node->ReactionIndex].Command;
				if (!Context->Command->IsComposite() && !FStageScenarioGraphAdapter(Data).Wrap(Context, USequenceStageMemberCommand::StaticClass())) return;
				Context->Command = Data->MemberReactions[Node->ReactionIndex].Command;
				Enter(Context);
			}
			else
			{
				if (!Node->Command || !Node->Command->IsComposite()) return;
				Enter(Node);
			}
		}
		else if (Node->Kind == EStageScenarioNodeKind::Reaction)
		{
			Enter(Node);
		}
		else
		{
			FStageViewScope S;
			S.bAssignments = true;
			S.Step = Node->Kind == EStageScenarioNodeKind::Finale ? FGuid() : FStageScenarioGraphAdapter::StepKey(Data, Node->StepIndex);
			S.Name = TEXT("Parallel performers");
			(ReactionsView ? ReactionScopes : StoryScopes).Add(S);
			Build(ReactionsView, true);
		}
		auto* Destination = ReactionsView ? Reactions.Get() : Story.Get();
		Action(Destination, EStageGraphAction::Add, Class, nullptr, FVector2D(80, 260));
		return;
	}
	if (Op == EStageGraphAction::Copy || Op == EStageGraphAction::Duplicate)
	{
		FString Text;
		if (!FStageGraphClipboard::Copy(Graph, Nodes, Text))
		{
			Message = TEXT("Select actions or blocks to copy.");
			return;
		}
		if (Op == EStageGraphAction::Copy)
		{
			FPlatformApplicationMisc::ClipboardCopy(*Text);
			return;
		}
		ItemDetails->SetStructureData(nullptr);
		CommandDetails->SetObject(nullptr);
		if (!FStageGraphClipboard::Paste(Graph, Text, Location + FVector2D(80, 100), true)) Message = TEXT("Duplicate conflicts with an existing performer or parallel resource.");
		QueueRefresh();
		return;
	}
	ItemDetails->SetStructureData(nullptr);
	CommandDetails->SetObject(nullptr);
	bool Done = false;
	FStageScenarioGraphAdapter Adapter(Data);
	if (Op == EStageGraphAction::Paste)
	{
		FString Text;
		FPlatformApplicationMisc::ClipboardPaste(Text);
		Done = FStageGraphClipboard::Paste(Graph, Text, Location, true);
	}
	else if (Op == EStageGraphAction::Collapse)
	{
		Done = FStageGraphOrganization::Collapse(Graph, Nodes, TEXT("Block"));
	}
	else if (Op == EStageGraphAction::Expand && Node)
		Done = FStageGraphOrganization::Expand(Graph, Node->GroupId);
	else if (Op == EStageGraphAction::Layout)
	{
		FStageGraphOrganization::Layout(Graph);
		Done = true;
	}
	else if (Production)
	{
		FStageProductionGraphAdapter P(Production);
		if (Op == EStageGraphAction::AddScene) Done = P.AddInView(Graph, Node, Location);
		if (Op == EStageGraphAction::AddPreparation && Node) Done = FStageGraphDrafts::AddPreparation(Graph, Node, Location + FVector2D(0, 160));
		if (Op == EStageGraphAction::RemovePreparation && Node) Done = P.Preparation(Node->StepIndex, false);
		if (Op == EStageGraphAction::Delete)
		{
			Done = FStageGraphDrafts::Delete(Graph, Nodes);
			Nodes.RemoveAll([](const auto* N)
			{
				return N->IsDraft();
			});
			Done = FStageGraphOrganization::Delete(Graph, Nodes) || Done;
		}
		if ((Op == EStageGraphAction::MoveEarlier || Op == EStageGraphAction::MoveLater) && Node)
		{
			int32 I = Graph->Nodes.IndexOfByKey(Node);
			int32 After = Op == EStageGraphAction::MoveEarlier ? I - 2 : I + 1;
			if (Graph->Nodes.IsValidIndex(After)) Done = P.MoveAfter(Graph, Node, CastChecked<UStageScenarioEdNode>(Graph->Nodes[After]));
		}
	}
	else
	{
		auto Roles = Data->GetRoleOptions();
		FName RoleId = Roles.IsEmpty() ? NAME_None : Roles[0];
		if (Node)
			if (auto* A = Adapter.Assignment(Node)) RoleId = A->GetEffectiveRoleId();
		if (Op == EStageGraphAction::Add || Op == EStageGraphAction::AddReaction)
			Done = Adapter.AddInView(Graph, Class, Node, Location);
		else if (Op == EStageGraphAction::AddParallelRole && Node)
		{
			const auto& Entries = Node->StepIndex == INDEX_NONE ? Data->Finale : Data->Steps[Node->StepIndex].Commands;
			FName FreeRole;
			for (FName Id : Roles)
				if (!Entries.ContainsByPredicate([&](const auto& A)
				{
					return A.GetEffectiveRoleId() == Id;
				}))
				{
					FreeRole = Id;
					break;
				}
			Done = Adapter.AddCommand(Node, Class, FreeRole, false);
		}
		else if (Op == EStageGraphAction::AddChild)
			Done = Adapter.AddCommand(Node, Class, RoleId, true);
		else if (Op == EStageGraphAction::Replace)
			Done = Adapter.ReplaceCommand(Node, Class);
		else if (Op == EStageGraphAction::WrapSequence || Op == EStageGraphAction::WrapParallel)
			Done = Adapter.Wrap(Node, Op == EStageGraphAction::WrapSequence ? USequenceStageMemberCommand::StaticClass() : UParallelStageMemberCommand::StaticClass());
		else if (Op == EStageGraphAction::RunSequence || Op == EStageGraphAction::RunParallel)
			Done = Adapter.SetContainerMode(Node, Op == EStageGraphAction::RunSequence ? USequenceStageMemberCommand::StaticClass() : UParallelStageMemberCommand::StaticClass());
		else if (Op == EStageGraphAction::Delete)
		{
			Done = FStageGraphDrafts::Delete(Graph, Nodes);
			Nodes.RemoveAll([](const auto* N)
			{
				return N->IsDraft();
			});
			Done = FStageGraphOrganization::Delete(Graph, Nodes) || Done;
		}
		else if (Op == EStageGraphAction::MoveEarlier || Op == EStageGraphAction::MoveLater)
		{
			if (Node)
			{
				const int32 I = Graph->Nodes.IndexOfByKey(Node);
				const int32 After = Op == EStageGraphAction::MoveEarlier ? I - 2 : I + 1;
				if (Graph->Nodes.IsValidIndex(After)) Done = Adapter.MoveElementsAfter(Graph, Node, CastChecked<UStageScenarioEdNode>(Graph->Nodes[After]));
			}
		}
	}
	if (!Done)
	{
		Message = TEXT("Edit unavailable: check selection, declared roles and dependencies. Collapse requires adjacent nodes; parallel actions need different resources.");
		return;
	}
	QueueRefresh();
}

void FStageScenarioEditorToolkit::Validate()
{
	TArray<FString> Errors;
	if (Data)
		Data->ValidateDefinition(Errors);
	else
		Production->ValidateDefinition(Errors);
	Message = Errors.IsEmpty()
	    ? TEXT("Right-click or drag Execute to add. Double-click blocks to open. Select adjacent nodes → Collapse to subgraph. Ctrl+Z: undo. Definition valid.")
	    : FString::Join(Errors, TEXT("\n"));
}

void FStageScenarioEditorToolkit::QueueRefresh()
{
	if (bRefreshQueued) return;
	bRefreshQueued = true;
	TWeakPtr<FStageScenarioEditorToolkit> Weak = StaticCastSharedRef<FStageScenarioEditorToolkit>(AsShared());
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
	{
		if (auto Self = Weak.Pin())
		{
			Self->bRefreshQueued = false;
			Self->Refresh();
		}
		return false;
	}));
}

void FStageScenarioEditorToolkit::Changed(UObject* Object, FPropertyChangedEvent& Event)
{
	if (Object
	    && (Object == Asset || Object->IsIn(Asset)
	        || (Production
	            && Production->Scenes.ContainsByPredicate([&](const auto& S)
	{
		return S.Scenario == Object;
	}))))
		QueueRefresh();
}

void FStageScenarioEditorToolkit::NotifyPreChange(FProperty* Property)
{
	if (Data)
	{
		FStageScenarioGraphAdapter(Data).EnsureIdentities();
		FStageGraphParameters::ModifyBound(Data);
	}
	else
		FStageProductionGraphAdapter(Production).EnsureIdentities();
	if (Selected.IsValid() && Selected->Command) Selected->Command->Modify();
}

void FStageScenarioEditorToolkit::NotifyPostChange(const FPropertyChangedEvent& Event, FProperty* Property)
{
	FStageGraphParameters::Synchronize(Data);
	Asset->MarkPackageDirty();
	QueueRefresh();
}

void FStageScenarioEditorToolkit::SaveAsset_Execute()
{
	if (!bEditable) return;
	Asset->Modify();
	for (auto* G : {Story.Get(), Reactions.Get()})
		if (G)
			if (auto Editor = View(G))
			{
				FVector2f Location;
				float Zoom;
				Editor->GetViewLocation(Location, Zoom);
				FStageGraphViewState State;
				State.Location = FVector2D(Location);
				State.Zoom = Zoom;
				G->Views().Add(G->ViewKey(), State);
			}
	Asset->MarkPackageDirty();
	FAssetEditorToolkit::SaveAsset_Execute();
}
