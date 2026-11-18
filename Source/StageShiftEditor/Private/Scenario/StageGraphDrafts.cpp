#include "StageGraphDrafts.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "Scenario/StageScenarioEdNode.h"
#include "StageScenarioGraphAdapter.h"
#include "StageProductionGraphAdapter.h"
#include "StageCommandCatalog.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "Stage/Core/StageMemberCommand.h"
#include "Stage/Core/StageCondition.h"
#include "Stage/Core/SequenceStageMemberCommand.h"
#include "Stage/Core/ParallelStageMemberCommand.h"
#include "Stage/Commands/StageWaitCommand.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

namespace
{
	TArray<TObjectPtr<UStageMemberCommand>>* Children(UStageMemberCommand* C)
	{
		if (auto* S = Cast<USequenceStageMemberCommand>(C)) return &S->Children;
		if (auto* P = Cast<UParallelStageMemberCommand>(C)) return &P->Children;
		return nullptr;
	}

	FName Unique(const TCHAR* Prefix, TFunctionRef<bool(FName)> Used)
	{
		for (int32 I = 1;; ++I)
		{
			FName Name(*FString::Printf(TEXT("%s%d"), Prefix, I));
			if (!Used(Name)) return Name;
		}
	}

	bool Begin(UStageScenarioEdGraph* G)
	{
		return G && !G->bReadOnly && FStageScenarioGraphAdapter::Backup(G->GetAsset());
	}

	void Changed(UStageScenarioEdGraph* G)
	{
		G->GetAsset()->MarkPackageDirty();
		G->GetAsset()->PostEditChange();
	}

	int32 InsertIndex(const UStageScenarioEdGraph* G, const UStageScenarioEdNode* Anchor, bool After, const TArray<FGuid>& Order)
	{
		TSet<FGuid> Visible;
		for (const UEdGraphNode* Base : G->Nodes)
			for (FGuid Id : CastChecked<UStageScenarioEdNode>(Base)->RepresentedIds)
				Visible.Add(Id);
		int32 Begin = 0, End = Order.Num();
		if (G->ActiveGroup.IsValid())
		{
			Begin = INDEX_NONE;
			End = 0;
			for (int32 I = 0; I < Order.Num(); ++I)
				if (Visible.Contains(Order[I]))
				{
					if (Begin == INDEX_NONE) Begin = I;
					End = I + 1;
				}
			if (Begin == INDEX_NONE) Begin = End;
		}
		if (Anchor->Kind == EStageScenarioNodeKind::Start) return Begin;
		if (Anchor->Kind == EStageScenarioNodeKind::Return || Anchor->Kind == EStageScenarioNodeKind::Finale) return End;
		int32 Found = INDEX_NONE;
		for (int32 I = 0; I < Order.Num(); ++I)
			if (Anchor->RepresentedIds.Contains(Order[I]))
			{
				Found = I;
				if (!After) break;
			}
		return Found == INDEX_NONE ? End : Found + (After ? 1 : 0);
	}
}

bool FStageGraphDrafts::Add(UStageScenarioEdGraph* G, UClass* Class, const UStageScenarioEdNode* Context, FVector2D Position)
{
	if (!Begin(G) || !G->Scenario) return false;
	auto* Data = G->Scenario.Get();
	FStageScenarioGraphAdapter A(Data);
	auto* Command = A.NewCommand(Class, G->ScopeCommand ? static_cast<UObject*>(G->ScopeCommand) : Data);
	if (!Command) return false;
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "AddUnconnected", "Add unconnected node"));
	A.EnsureIdentities();
	FStageGraphDraft D;
	D.Id = FGuid::NewGuid();
	D.Scope = G->ScopeId;
	D.Group = G->ActiveGroup;
	D.Kind = G->ScopeCommand                                 ? EStageGraphDraftKind::Child
	    : G->bParallelAssignments || G->bReactionAssignments ? EStageGraphDraftKind::Assignment
	    : G->bReactions                                      ? EStageGraphDraftKind::Reaction
	                                                         : EStageGraphDraftKind::Step;
	const auto Roles = Data->GetRoleOptions();
	FName Role = Roles.IsEmpty() ? NAME_None : Roles[0];
	if (auto* Entry = A.Assignment(Context)) Role = Entry->GetEffectiveRoleId();
	if (D.Kind == EStageGraphDraftKind::Assignment)
	{
		const auto Entries = G->bReactionAssignments ? Data->MemberReactions[G->ScopeReaction].GetAssignments()
		    : G->ScopeStep == INDEX_NONE             ? Data->Finale
		                                             : Data->Steps[G->ScopeStep].Commands;
		for (FName Candidate : Roles)
			if (!Entries.ContainsByPredicate([&](const auto& V)
			{
				return V.GetEffectiveRoleId() == Candidate;
			}))
			{
				Role = Candidate;
				break;
			}
	}
	if (G->ScopeCommand)
	{
		TSet<const UStageMemberCommand*> Seen;
		TFunction<bool(const UStageMemberCommand*)> Contains = [&](const UStageMemberCommand* C)
		{
			if (C == G->ScopeCommand) return true;
			if (!C || Seen.Contains(C)) return false;
			Seen.Add(C);
			for (const UStageMemberCommand* Child : C->GetChildCommands())
				if (Contains(Child)) return true;
			return false;
		};
		for (const auto& Step : Data->Steps)
			for (const auto& A1 : Step.Commands)
				if (Contains(A1.Command)) Role = A1.GetEffectiveRoleId();
		for (const auto& A1 : Data->Finale)
			if (Contains(A1.Command)) Role = A1.GetEffectiveRoleId();
		for (const auto& R : Data->MemberReactions)
			for (const auto& A1 : R.GetAssignments())
				if (Contains(A1.Command)) Role = A1.GetEffectiveRoleId();
		for (const auto& Draft : Data->EditorDrafts)
			if (Contains(Draft.GetCommand()))
				Role = Draft.Kind == EStageGraphDraftKind::Reaction ? Draft.Reaction.GetEffectiveRoleId()
				    : Draft.Step.Commands.IsEmpty()                 ? Role
				                                                    : Draft.Step.Commands[0].GetEffectiveRoleId();
	}
	D.Step.StepId = Unique(TEXT("Step"), [&](FName Name)
	{
		return Data->Steps.ContainsByPredicate(
		           [&](const auto& S)
		{
			return S.StepId == Name;
		})
		    || Data->EditorDrafts.ContainsByPredicate([&](const auto& V)
		{
			return V.Step.StepId == Name;
		});
	});
	D.Step.EditorGuid = D.Id;
	FStageMemberAssignment Entry;
	Entry.RoleId = Role;
	Entry.Command = Command;
	D.Step.Commands.Add(Entry);
	if (D.Kind == EStageGraphDraftKind::Reaction)
	{
		D.Reaction.ReactionId = Unique(TEXT("Reaction"), [&](FName Name)
		{
			return Data->MemberReactions.ContainsByPredicate(
			           [&](const auto& R)
			{
				return R.ReactionId == Name;
			})
			    || Data->EditorDrafts.ContainsByPredicate([&](const auto& V)
			{
				return V.Reaction.ReactionId == Name;
			});
		});
		D.Reaction.RoleId = Role;
		D.Reaction.Command = Command;
		D.Reaction.EditorGuid = D.Id;
		D.Reaction.EventTag = FGameplayTag::RequestGameplayTag(TEXT("State.Member.Fallen"), false);
		D.Step.Commands.Reset();
	}
	Data->EditorDrafts.Add(D);
	G->Positions().Add(D.Id, Position);
	Changed(G);
	return true;
}

bool FStageGraphDrafts::AddBranch(UStageScenarioEdGraph* G, const UStageScenarioEdNode* Context, FVector2D Position)
{
	if (!G || !G->IsStorySteps() || !Begin(G)) return false;
	auto* Data = G->Scenario.Get();
	FStageScenarioGraphAdapter Adapter(Data);
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "AddBranch", "Add Branch"));
	Adapter.EnsureIdentities();
	auto& Draft = Data->EditorDrafts.AddDefaulted_GetRef();
	Draft.Id = Draft.Step.EditorGuid = FGuid::NewGuid();
	Draft.Scope = G->ScopeId;
	Draft.Group = G->ActiveGroup;
	Draft.Kind = EStageGraphDraftKind::Step;
	Draft.Step.StepId = Unique(TEXT("Branch"), [&](FName Id)
	{
		return Data->FindStepIndex(Id) != INDEX_NONE || Data->EditorDrafts.ContainsByPredicate([Id](const auto& Item) { return Item.Step.StepId == Id; });
	});
	Draft.Step.Branch = NewObject<UStageBranch>(Data, NAME_None, RF_Transactional);
	auto* Condition = NewObject<UStageHasHeldItemCondition>(Draft.Step.Branch, NAME_None, RF_Transactional);
	const auto Roles = Data->GetRoleOptions();
	if (const auto* Assignment = Adapter.Assignment(Context)) Condition->RoleId = Assignment->GetEffectiveRoleId();
	else if (!Roles.IsEmpty()) Condition->RoleId = Roles[0];
	Draft.Step.Branch->Condition = Condition;
	G->Positions().Add(Draft.Id, Position);
	Changed(G);
	return true;
}

bool FStageGraphDrafts::AddScene(UStageScenarioEdGraph* G, FVector2D Position)
{
	if (!Begin(G) || !G->Production) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "DraftScene", "Add unconnected scene"));
	FStageProductionGraphAdapter(G->Production).EnsureIdentities();
	FStageProductionDraft D;
	D.Id = FGuid::NewGuid();
	D.Scope = G->ScopeId;
	D.Group = G->ActiveGroup;
	D.Scene.EditorGuid = D.Id;
	D.Scene.PreparationGuid = FGuid::NewGuid();
	D.Scene.SceneId = Unique(TEXT("Scene"), [&](FName Name)
	{
		return G->Production->Scenes.ContainsByPredicate(
		           [&](const auto& S)
		{
			return S.SceneId == Name;
		})
		    || G->Production->EditorDrafts.ContainsByPredicate([&](const auto& V)
		{
			return V.Scene.SceneId == Name;
		});
	});
	G->Production->EditorDrafts.Add(D);
	G->Positions().Add(D.Id, Position);
	Changed(G);
	return true;
}

bool FStageGraphDrafts::AddPreparation(UStageScenarioEdGraph* G, const UStageScenarioEdNode* Node, FVector2D Position)
{
	if (!Begin(G) || !G->Production || !Node || Node->IsDraft() || !G->Production->Scenes.IsValidIndex(Node->StepIndex)) return false;
	const FGuid Target = FStageProductionGraphAdapter::SceneKey(G->Production, Node->StepIndex);
	if (G->Production->Scenes[Node->StepIndex].bPrepareBefore
	    || G->Production->EditorDrafts.ContainsByPredicate([&](const auto& D)
	{
		return D.bPreparationOnly && D.TargetScene == Target;
	}))
		return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "DraftPrep", "Add unconnected preparation"));
	FStageProductionGraphAdapter(G->Production).EnsureIdentities();
	FStageProductionDraft D;
	D.Id = FGuid::NewGuid();
	D.Scope = G->ScopeId;
	D.Group = G->ActiveGroup;
	D.TargetScene = Target;
	D.bPreparationOnly = true;
	D.Scene = G->Production->Scenes[Node->StepIndex];
	D.Scene.bPrepareBefore = true;
	D.Scene.PreparationGuid = D.Id;
	G->Production->EditorDrafts.Add(D);
	G->Positions().Add(D.Id, Position);
	Changed(G);
	return true;
}

void FStageGraphDrafts::Project(UStageScenarioEdGraph* G)
{
	auto IsInside = [&](FGuid Group, FGuid Parent)
	{
		TSet<FGuid> Seen;
		while (Group.IsValid() && !Seen.Contains(Group))
		{
			if (Group == Parent) return true;
			Seen.Add(Group);
			const auto* Item = G->Groups().FindByPredicate([&](const auto& V)
			{
				return V.Id == Group;
			});
			Group = Item ? Item->Parent : FGuid();
		}
		return false;
	};
	for (const auto& Group : G->Groups())
		if (Group.Scope == G->ScopeId && Group.Parent == G->ActiveGroup
		    && !G->Nodes.ContainsByPredicate([&](const UEdGraphNode* N)
		{
			return N->NodeGuid == Group.Id;
		}))
		{
			bool HasDrafts = false;
			if (G->Scenario)
				for (const auto& D : G->Scenario->EditorDrafts)
					HasDrafts |= D.Scope == Group.Scope && IsInside(D.Group, Group.Id);
			if (G->Production)
				for (const auto& D : G->Production->EditorDrafts)
					HasDrafts |= D.Scope == Group.Scope && IsInside(D.Group, Group.Id);
			if (!HasDrafts) continue;
			auto* N = NewObject<UStageScenarioEdNode>(G, NAME_None, RF_Transactional);
			N->Kind = EStageScenarioNodeKind::Group;
			N->GroupId = Group.Id;
			N->NodeGuid = Group.Id;
			N->Title = FText::FromString(Group.Title);
			N->Summary = TEXT("Unconnected actions · double-click to edit");
			N->bCanRenameNode = true;
			N->AllocateDefaultPins();
			const auto P = G->Positions().FindRef(N->NodeGuid);
			N->NodePosX = P.X;
			N->NodePosY = P.Y;
			G->AddNode(N);
		}
	auto Make = [&](int32 Index, FGuid Id, EStageScenarioNodeKind Kind, const FString& Title, UStageMemberCommand* Command)
	{
		auto* N = NewObject<UStageScenarioEdNode>(G, NAME_None, RF_Transactional);
		N->DraftIndex = Index;
		N->NodeGuid = Id;
		N->Kind = Kind;
		N->Title = FText::FromString(Title);
		N->Command = Command;
		N->Summary = TEXT("Not connected · does not execute");
		N->AllocateDefaultPins();
		const FVector2D P = G->Positions().FindRef(Id);
		N->NodePosX = P.X;
		N->NodePosY = P.Y;
		G->AddNode(N);
		return N;
	};
	if (G->Scenario)
		for (int32 I = 0; I < G->Scenario->EditorDrafts.Num(); ++I)
		{
			const auto& D = G->Scenario->EditorDrafts[I];
			if (D.Scope != G->ScopeId || D.Group != G->ActiveGroup) continue;
			if (D.Step.Branch)
			{
				Make(I, D.Id, EStageScenarioNodeKind::Branch, TEXT("Branch"), nullptr);
				continue;
			}
			auto* C = D.GetCommand();
			Make(I, D.Id, EStageScenarioNodeKind::Command, C ? FStageCommandCatalog::Name(C->GetClass()).ToString() : TEXT("Missing action"), C);
			if (D.Kind == EStageGraphDraftKind::Reaction)
			{
				const FGuid Id = EventId(D.Id);
				auto* Event = Make(I, Id, EStageScenarioNodeKind::Reaction, D.Reaction.EventTag.ToString(), nullptr);
				if (!G->Positions().Contains(Id))
				{
					const auto P = G->Positions().FindRef(D.Id);
					Event->NodePosX = P.X - 300;
					Event->NodePosY = P.Y;
				}
				Event->Summary = D.Reaction.ReactionId.ToString() + TEXT(" · not connected");
			}
		}
	if (G->Production)
		for (int32 I = 0; I < G->Production->EditorDrafts.Num(); ++I)
		{
			const auto& D = G->Production->EditorDrafts[I];
			if (D.Scope != G->ScopeId || D.Group != G->ActiveGroup) continue;
			Make(I, D.Id, D.bPreparationOnly ? EStageScenarioNodeKind::Preparation : EStageScenarioNodeKind::Scene,
			    (D.bPreparationOnly ? TEXT("Prepare · ") : TEXT("Scene · ")) + D.Scene.SceneId.ToString(), nullptr);
		}
}

bool FStageGraphDrafts::Connect(UStageScenarioEdGraph* G, const UStageScenarioEdNode* To, const UStageScenarioEdNode* From, bool bOnFailed)
{
	if (!Begin(G) || !To || !From) return false;
	const auto* Node = To->IsDraft() ? To : From->IsDraft() ? From : nullptr;
	if (!Node) return false;
	if (G->Production)
	{
		if (!G->Production->EditorDrafts.IsValidIndex(Node->DraftIndex)) return false;
		const auto D = G->Production->EditorDrafts[Node->DraftIndex];
		const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "ConnectSceneDraft", "Connect scene node"));
		FStageProductionGraphAdapter(G->Production).EnsureIdentities();
		if (D.bPreparationOnly)
		{
			const int32 Index = G->Production->Scenes.IndexOfByPredicate([&](const auto& S)
			{
				return S.EditorGuid == D.TargetScene;
			});
			if (Index == INDEX_NONE) return false;
			auto& S = G->Production->Scenes[Index];
			S.bPrepareBefore = true;
			S.PreparationGuid = D.Id;
			S.PreparationPrompt = D.Scene.PreparationPrompt;
			S.bAutoStartWhenReady = D.Scene.bAutoStartWhenReady;
			S.PreparationTimeout = D.Scene.PreparationTimeout;
			S.Placements = D.Scene.Placements;
		}
		else
		{
			const auto* Anchor = Node == To ? From : To;
			if (Anchor->IsDraft()) return false;
			TArray<FGuid> Order;
			for (const auto& S : G->Production->Scenes)
				Order.Add(S.EditorGuid);
			const int32 Index = InsertIndex(G, Anchor, Node == To, Order);
			G->Production->Scenes.Insert(D.Scene, Index);
		}
		if (D.Group.IsValid())
			if (auto* Group = G->Groups().FindByPredicate([&](const auto& V)
			{
				return V.Id == D.Group;
			}))
				Group->Members.AddUnique(D.Id);
		G->Production->EditorDrafts.RemoveAt(Node->DraftIndex);
		Changed(G);
		return true;
	}
	auto* Data = G->Scenario.Get();
	if (!Data || !Data->EditorDrafts.IsValidIndex(Node->DraftIndex)) return false;
	const auto D = Data->EditorDrafts[Node->DraftIndex];
	FStageScenarioGraphAdapter A(Data);
	const auto* Anchor = Node == To ? From : To;
	if (Anchor->IsDraft() && !(D.Kind == EStageGraphDraftKind::Reaction && Anchor->DraftIndex == Node->DraftIndex && From->Kind == EStageScenarioNodeKind::Reaction)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "ConnectDraft", "Connect action node"));
	A.EnsureIdentities();
	FGuid ConnectedId = D.Id;
	if (D.Kind == EStageGraphDraftKind::Step)
	{
		if (!D.Step.Branch && (D.Step.Commands.IsEmpty() || !Data->GetRoleOptions().Contains(D.Step.Commands[0].GetEffectiveRoleId()))) return false;
		TArray<FGuid> Order;
		for (const auto& S : Data->Steps)
			Order.Add(S.EditorGuid);
		const int32 Index = InsertIndex(G, Anchor, Node == To, Order);
		auto Step = D.Step;
		if (Data->Steps.ContainsByPredicate([&](const auto& S)
		{
			return S.StepId == Step.StepId;
		}))
			Step.StepId = Unique(Step.Branch ? TEXT("Branch") : TEXT("Step"), [&](FName Name)
			{
				return Data->Steps.ContainsByPredicate([&](const auto& S)
				{
					return S.StepId == Name;
				});
			});
		if (Data->bUseStepLinks || bOnFailed || Step.Branch)
		{
			if (Node != To || (!Data->Steps.IsValidIndex(From->StepIndex) && From->Kind != EStageScenarioNodeKind::Start)) return false;
			A.EnableStepLinks();
			Step.FailedStepId = NAME_None;
			if (From->Kind == EStageScenarioNodeKind::Start)
			{
				Step.ThenStepId = Data->EntryStepId;
				Data->EntryStepId = Step.StepId;
			}
			else
			{
				FStageStep& Source = Data->Steps[From->StepIndex];
				FName& Link = bOnFailed ? Source.FailedStepId : Source.ThenStepId;
				// Insert the new Step on this exit, retaining its original destination.
				Step.ThenStepId = bOnFailed && Link.IsNone() && !Source.Branch ? Source.ThenStepId : Link;
				Link = Step.StepId;
			}
			Data->Steps.Add(Step);
		}
		else
		{
			Data->Steps.Insert(Step, Index);
		}
		if (Data->Finale.IsEmpty() && !Step.Branch)
		{
			auto Entry = D.Step.Commands[0];
			Entry.Command = A.NewCommand(UStageWaitCommand::StaticClass(), Data);
			FindFProperty<FFloatProperty>(Entry.Command->GetClass(), TEXT("WaitTime"))->SetPropertyValue_InContainer(Entry.Command, 0);
			Data->Finale.Add(Entry);
		}
	}
	else if (D.Kind == EStageGraphDraftKind::Reaction)
	{
		Data->MemberReactions.Add(D.Reaction);
		ConnectedId = A.Key(D.Reaction.Command);
	}
	else if (D.Kind == EStageGraphDraftKind::Child)
	{
		auto* Array = Children(G->ScopeCommand);
		auto* C = D.GetCommand();
		if (!Array || !C) return false;
		if (G->ScopeCommand->IsA<UParallelStageMemberCommand>())
			for (const UStageMemberCommand* Sibling : *Array)
				if (Sibling && (Sibling->GetResourceMask() & C->GetResourceMask())) return false;
		G->ScopeCommand->Modify();
		TArray<FGuid> Order;
		for (UStageMemberCommand* Child : *Array)
			Order.Add(A.Key(Child));
		const int32 Index = G->bLinear ? InsertIndex(G, Anchor, Node == To, Order) : Array->Num();
		Array->Insert(C, Index);
		ConnectedId = A.Key(C);
	}
	else
	{
		const auto Entry = D.Step.Commands[0];
		const auto Entries = G->bReactionAssignments ? Data->MemberReactions[G->ScopeReaction].GetAssignments()
		    : G->ScopeStep == INDEX_NONE             ? Data->Finale
		                                             : Data->Steps[G->ScopeStep].Commands;
		if (!Data->GetRoleOptions().Contains(Entry.GetEffectiveRoleId())
		    || Entries.ContainsByPredicate([&](const auto& E)
		{
			return E.GetEffectiveRoleId() == Entry.GetEffectiveRoleId();
		}))
			return false;
		if (G->bReactionAssignments)
			Data->MemberReactions[G->ScopeReaction].AdditionalPerformers.Add(Entry);
		else
			(G->ScopeStep == INDEX_NONE ? Data->Finale : Data->Steps[G->ScopeStep].Commands).Add(Entry);
		ConnectedId = A.Key(Entry.Command);
	}
	if (D.Group.IsValid())
		if (auto* Group = G->Groups().FindByPredicate([&](const auto& V)
		{
			return V.Id == D.Group;
		}))
			Group->Members.AddUnique(ConnectedId);
	G->Positions().Add(ConnectedId, G->Positions().FindRef(D.Id));
	Data->EditorDrafts.RemoveAt(Node->DraftIndex);
	Changed(G);
	return true;
}

bool FStageGraphDrafts::Delete(UStageScenarioEdGraph* G, const TArray<UStageScenarioEdNode*>& Nodes)
{
	TSet<int32> Indices;
	for (auto* N : Nodes)
		if (N && N->IsDraft()) Indices.Add(N->DraftIndex);
	if (Indices.IsEmpty() || !Begin(G)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "DeleteDraft", "Delete unconnected nodes"));
	G->GetAsset()->Modify();
	if (G->Scenario)
	{
		TArray<UStageMemberCommand*> Roots;
		for (int32 I : Indices)
			if (G->Scenario->EditorDrafts.IsValidIndex(I)) Roots.Add(G->Scenario->EditorDrafts[I].GetCommand());
		for (int32 I = G->Scenario->EditorDrafts.Num() - 1; I >= 0; --I)
			if (Indices.Contains(I)
			    || Roots.ContainsByPredicate([&](const auto* Root)
			{
				const auto* C = G->Scenario->EditorDrafts[I].GetCommand();
				return Root && C && C->IsIn(Root);
			}))
				G->Scenario->EditorDrafts.RemoveAt(I);
	}
	if (G->Production)
		for (int32 I = G->Production->EditorDrafts.Num() - 1; I >= 0; --I)
			if (Indices.Contains(I)) G->Production->EditorDrafts.RemoveAt(I);
	Changed(G);
	return true;
}

bool FStageGraphDrafts::SetRole(UStageScenarioEdGraph* G, const UStageScenarioEdNode* Node, FName Role, bool bSubject)
{
	if (!Begin(G) || !Node || !G->Scenario || !G->Scenario->EditorDrafts.IsValidIndex(Node->DraftIndex)
	    || (!G->Scenario->GetRoleOptions().Contains(Role) && !(bSubject && Role.IsNone())))
		return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "DraftRole", "Set unconnected action role"));
	G->Scenario->Modify();
	auto& D = G->Scenario->EditorDrafts[Node->DraftIndex];
	if (D.Kind == EStageGraphDraftKind::Reaction)
	{
		if (bSubject)
			D.Reaction.SubjectRoleId = Role;
		else
			D.Reaction.RoleId = Role;
	}
	else if (!bSubject && !D.Step.Commands.IsEmpty())
		D.Step.Commands[0].RoleId = Role;
	Changed(G);
	return true;
}
