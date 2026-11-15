#include "StageGraphClipboard.h"
#include "Scenario/StageGraphClipboardNode.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "Scenario/StageScenarioEdNode.h"
#include "StageScenarioGraphAdapter.h"
#include "StageProductionGraphAdapter.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/SequenceStageMemberCommand.h"
#include "Stage/Core/ParallelStageMemberCommand.h"
#include "EdGraphUtilities.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

namespace
{
	const FString Prefix = TEXT("StageShiftGraphClipboard/1\n");

	FName Unique(const TCHAR* Label, TFunctionRef<bool(FName)> Used)
	{
		for (int32 I = 1;; ++I)
		{
			FName Id(*FString::Printf(TEXT("%s%d"), Label, I));
			if (!Used(Id)) return Id;
		}
	}

	TArray<TObjectPtr<UStageMemberCommand>>* Children(UStageMemberCommand* C)
	{
		if (auto* S = Cast<USequenceStageMemberCommand>(C)) return &S->Children;
		if (auto* P = Cast<UParallelStageMemberCommand>(C)) return &P->Children;
		return nullptr;
	}
}

bool FStageGraphClipboard::Copy(UStageScenarioEdGraph* Graph, const TArray<UStageScenarioEdNode*>& Nodes, FString& Text)
{
	if (!Graph || Nodes.IsEmpty()) return false;
	auto* Temp = NewObject<UEdGraph>();
	auto* Payload = NewObject<UStageGraphClipboardNode>(Temp);
	Temp->AddNode(Payload);
	TSet<FGuid> Ids;
	TSet<int32> Indices, Drafts;
	Payload->Parameters = Graph->Scenario ? Graph->Scenario->Parameters : Graph->Production->Parameters;
	for (auto* N : Nodes)
	{
		if (!N || !N->CanDuplicateNode()) continue;
		if (N->IsDraft())
		{
			Drafts.Add(N->DraftIndex);
			continue;
		}
		for (FGuid Id : N->RepresentedIds)
			Ids.Add(Id);
		if (N->ReactionIndex != INDEX_NONE) Indices.Add(N->ReactionIndex);
	}
	auto CopyCommand = [&](UStageMemberCommand* C)
	{
		return C ? DuplicateObject<UStageMemberCommand>(C, Payload) : nullptr;
	};
	if (Graph->Production)
	{
		Payload->Mode = 4;
		for (int32 I = 0; I < Graph->Production->Scenes.Num(); ++I)
		{
			auto S = Graph->Production->Scenes[I];
			S.EditorGuid = FStageProductionGraphAdapter::SceneKey(Graph->Production, I);
			S.PreparationGuid = FStageProductionGraphAdapter::SceneKey(Graph->Production, I, true);
			if (Ids.Contains(S.EditorGuid))
			{
				Payload->Scenes.Add(S);
				if (S.bPrepareBefore) Ids.Add(S.PreparationGuid);
			}
		}
	}
	else if (Graph->ScopeCommand)
	{
		Payload->Mode = 2;
		for (UStageMemberCommand* C : Graph->ScopeCommand->GetChildCommands())
			if (Ids.Contains(FStageScenarioGraphAdapter::Key(C))) Payload->Commands.Add(CopyCommand(C));
	}
	else if (Graph->bParallelAssignments || Graph->bReactionAssignments)
	{
		Payload->Mode = 5;
		const auto Entries = Graph->bReactionAssignments ? Graph->Scenario->MemberReactions[Graph->ScopeReaction].GetAssignments()
		    : Graph->ScopeStep == INDEX_NONE             ? Graph->Scenario->Finale
		                                                 : Graph->Scenario->Steps[Graph->ScopeStep].Commands;
		for (auto A : Entries)
			if (Ids.Contains(FStageScenarioGraphAdapter::Key(A.Command)))
			{
				A.Command = CopyCommand(A.Command);
				Payload->Assignments.Add(A);
			}
	}
	else if (Graph->bReactions)
	{
		Payload->Mode = 3;
		for (int32 I = 0; I < Graph->Scenario->MemberReactions.Num(); ++I)
			if (Indices.Contains(I))
			{
				auto R = Graph->Scenario->MemberReactions[I];
				R.Command = CopyCommand(R.Command);
				for (auto& A : R.AdditionalPerformers)
					A.Command = CopyCommand(A.Command);
				Payload->Reactions.Add(R);
			}
	}
	else
	{
		Payload->Mode = 1;
		for (int32 I = 0; I < Graph->Scenario->Steps.Num(); ++I)
			if (Ids.Contains(FStageScenarioGraphAdapter::StepKey(Graph->Scenario, I)))
			{
				auto S = Graph->Scenario->Steps[I];
				S.EditorGuid = FStageScenarioGraphAdapter::StepKey(Graph->Scenario, I);
				for (auto& A : S.Commands)
					A.Command = CopyCommand(A.Command);
				Payload->Steps.Add(S);
			}
	}
	for (int32 Index : Drafts)
	{
		if (Graph->Production)
		{
			if (Graph->Production->EditorDrafts.IsValidIndex(Index) && !Graph->Production->EditorDrafts[Index].bPreparationOnly)
				Payload->Scenes.Add(Graph->Production->EditorDrafts[Index].Scene);
			continue;
		}
		if (!Graph->Scenario->EditorDrafts.IsValidIndex(Index)) continue;
		const auto& D = Graph->Scenario->EditorDrafts[Index];
		if (Payload->Mode == 1)
		{
			auto S = D.Step;
			for (auto& A : S.Commands)
				A.Command = CopyCommand(A.Command);
			Payload->Steps.Add(S);
		}
		if (Payload->Mode == 2) Payload->Commands.Add(CopyCommand(D.GetCommand()));
		if (Payload->Mode == 3)
		{
			auto R = D.Reaction;
			R.Command = CopyCommand(R.Command);
			for (auto& A : R.AdditionalPerformers)
				A.Command = CopyCommand(A.Command);
			Payload->Reactions.Add(R);
		}
		if (Payload->Mode == 5 && !D.Step.Commands.IsEmpty())
		{
			auto A = D.Step.Commands[0];
			A.Command = CopyCommand(A.Command);
			Payload->Assignments.Add(A);
		}
	}
	if (Payload->Steps.IsEmpty() && Payload->Commands.IsEmpty() && Payload->Reactions.IsEmpty() && Payload->Assignments.IsEmpty() && Payload->Scenes.IsEmpty()) return false;
	TSet<FGuid> SelectedGroups;
	for (auto* N : Nodes)
		if (N && N->Kind == EStageScenarioNodeKind::Group) SelectedGroups.Add(N->GroupId);
	bool Added = true;
	while (Added)
	{
		Added = false;
		for (const auto& G : Graph->Groups())
			if (SelectedGroups.Contains(G.Parent) && !SelectedGroups.Contains(G.Id))
			{
				SelectedGroups.Add(G.Id);
				Added = true;
			}
	}
	TSet<FGuid> OwnedScopes;
	TSet<UStageMemberCommand*> Seen;
	TFunction<void(UStageMemberCommand*)> Visit = [&](UStageMemberCommand* C)
	{
		if (!C || Seen.Contains(C)) return;
		Seen.Add(C);
		if (C->IsComposite()) OwnedScopes.Add(FStageScenarioGraphAdapter::Key(C));
		for (UStageMemberCommand* Child : C->GetChildCommands())
			Visit(Child);
	};
	for (const auto& S : Payload->Steps)
		for (const auto& A : S.Commands)
			Visit(A.Command);
	for (UStageMemberCommand* C : Payload->Commands)
		Visit(C);
	for (const auto& R : Payload->Reactions)
		for (const auto& A : R.GetAssignments())
			Visit(A.Command);
	for (const auto& A : Payload->Assignments)
		Visit(A.Command);
	for (auto G : Graph->Groups())
		if (SelectedGroups.Contains(G.Id) || OwnedScopes.Contains(G.Scope))
		{
			if (!SelectedGroups.Contains(G.Parent) && !OwnedScopes.Contains(G.Scope)) G.Parent.Invalidate();
			Payload->Groups.Add(G);
		}
	FString Export;
	FEdGraphUtilities::ExportNodesToText({Payload}, Export);
	Text = Prefix + Export;
	return true;
}

bool FStageGraphClipboard::Paste(UStageScenarioEdGraph* Graph, const FString& Text, FVector2D Location, bool bDisconnected)
{
	if (!Graph || Graph->bReadOnly || !Text.StartsWith(Prefix) || !FStageScenarioGraphAdapter::Backup(Graph->GetAsset())) return false;
	auto* Temp = NewObject<UEdGraph>();
	TSet<UEdGraphNode*> Imported;
	FEdGraphUtilities::ImportNodesFromText(Temp, Text.Mid(Prefix.Len()), Imported);
	if (Imported.Num() != 1) return false;
	auto* P = Cast<UStageGraphClipboardNode>(*Imported.CreateConstIterator());
	if (!P) return false;
	const int32 Mode = Graph->Production ? 4 : Graph->ScopeCommand ? 2 : Graph->bParallelAssignments || Graph->bReactionAssignments ? 5 : Graph->bReactions ? 3 : 1;
	if (P->Mode != Mode) return false;
	auto* Data = Graph->Scenario.Get();
	auto* ChildArray = Children(Graph->ScopeCommand);
	if (Mode == 2 && !ChildArray) return false;
	auto HasRole = [&](FName Id)
	{
		return Data && Data->GetRoleOptions().Contains(Id);
	};
	for (const auto& S : P->Steps)
		for (const auto& A : S.Commands)
			if (!HasRole(A.GetEffectiveRoleId())) return false;
	for (const auto& R : P->Reactions)
	{
		for (const auto& A : R.GetAssignments())
			if (!HasRole(A.GetEffectiveRoleId())) return false;
		if (!R.GetEffectiveSubjectRoleId().IsNone() && !HasRole(R.GetEffectiveSubjectRoleId())) return false;
	}
	TArray<FStageMemberAssignment>* AssignmentArray = Mode == 5 ? (Graph->bReactionAssignments             ? &Data->MemberReactions[Graph->ScopeReaction].AdditionalPerformers
	                                                                      : Graph->ScopeStep == INDEX_NONE ? &Data->Finale
	                                                                                                       : &Data->Steps[Graph->ScopeStep].Commands)
	                                                            : nullptr;
	if (AssignmentArray)
		for (const auto& A : P->Assignments)
			if (!HasRole(A.GetEffectiveRoleId())
			    || (!bDisconnected
			        && AssignmentArray->ContainsByPredicate([&](const auto& V)
			{
				return V.GetEffectiveRoleId() == A.GetEffectiveRoleId();
			})))
				return false;
	if (!bDisconnected && Mode == 2 && Graph->ScopeCommand->IsA<UParallelStageMemberCommand>())
	{
		uint8 Mask = Graph->ScopeCommand->GetResourceMask();
		for (UStageMemberCommand* C : P->Commands)
		{
			if (!C || (Mask & C->GetResourceMask())) return false;
			Mask |= C->GetResourceMask();
		}
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Paste", "Paste scenario nodes"));
	Graph->GetAsset()->Modify();
	if (Data)
		FStageScenarioGraphAdapter(Data).EnsureIdentities();
	else
		FStageProductionGraphAdapter(Graph->Production).EnsureIdentities();
	TMap<FGuid, FGuid> Remap, CommandRemap, ParameterRemap;
	TArray<FGuid> NewIds;
	auto& DestinationParameters = Data ? Data->Parameters : Graph->Production->Parameters;
	if (const UPropertyBag* Bag = P->Parameters.GetPropertyBagStruct())
		for (const auto& Desc : Bag->GetPropertyDescs())
		{
			const FPropertyBagPropertyDesc* Existing = DestinationParameters.FindPropertyDescByID(Desc.ID);
			if (!Existing) Existing = DestinationParameters.FindPropertyDescByName(Desc.Name);
			if (Existing && Existing->CachedProperty && Desc.CachedProperty && Existing->CachedProperty->SameType(Desc.CachedProperty))
			{
				ParameterRemap.Add(Desc.ID, Existing->ID);
				continue;
			}
			FName Name = Desc.Name;
			for (int32 I = 2; DestinationParameters.FindPropertyDescByName(Name); ++I)
				Name = FName(*(Desc.Name.ToString() + FString::FromInt(I)));
			DestinationParameters.AddProperty(Name, Desc.CachedProperty);
			const auto* Added = DestinationParameters.FindPropertyDescByName(Name);
			if (Added && Added->CachedProperty && Desc.CachedProperty)
			{
				Added->CachedProperty->CopyCompleteValue(Added->CachedProperty->ContainerPtrToValuePtr<void>(DestinationParameters.GetMutableValue().GetMemory()),
				    Desc.CachedProperty->ContainerPtrToValuePtr<void>(P->Parameters.GetValue().GetMemory()));
				ParameterRemap.Add(Desc.ID, Added->ID);
			}
		}
	TSet<FGuid> Visible;
	for (const UEdGraphNode* Base : Graph->Nodes)
		for (FGuid Id : CastChecked<UStageScenarioEdNode>(Base)->RepresentedIds)
			Visible.Add(Id);
	int32 Insert = Mode == 1 ? Data->Steps.Num() : Mode == 2 ? ChildArray->Num() : Mode == 4 ? Graph->Production->Scenes.Num() : 0;
	if (Graph->ActiveGroup.IsValid())
	{
		if (Mode == 1)
			for (int32 I = 0; I < Data->Steps.Num(); ++I)
				if (Visible.Contains(Data->Steps[I].EditorGuid)) Insert = I + 1;
		if (Mode == 2)
			for (int32 I = 0; I < ChildArray->Num(); ++I)
				if (Visible.Contains(FStageScenarioGraphAdapter::Key((*ChildArray)[I]))) Insert = I + 1;
		if (Mode == 4)
			for (int32 I = 0; I < Graph->Production->Scenes.Num(); ++I)
				if (Visible.Contains(Graph->Production->Scenes[I].EditorGuid)) Insert = I + 1;
	}
	auto Duplicate = [&](UStageMemberCommand* C, UObject* Outer)
	{
		auto* Copy = DuplicateObject<UStageMemberCommand>(C, Outer);
		FStageScenarioGraphAdapter::RenewGuids(Copy);
		TFunction<void(UStageMemberCommand*, UStageMemberCommand*)> Map = [&](UStageMemberCommand* Old, UStageMemberCommand* New)
		{
			const FGuid Key = FStageScenarioGraphAdapter::Key(Old);
			CommandRemap.Add(Key, New->EditorGuid);
			if (!Remap.Contains(Key)) Remap.Add(Key, New->EditorGuid);
			for (auto& Binding : New->GraphParameterBindings)
				if (const auto* Id = ParameterRemap.Find(Binding.Value)) Binding.Value = *Id;
			const auto& A = Old->GetChildCommands();
			const auto& B = New->GetChildCommands();
			for (int32 I = 0; I < A.Num(); ++I)
				if (A[I] && B.IsValidIndex(I) && B[I]) Map(A[I], B[I]);
		};
		Map(C, Copy);
		return Copy;
	};
	if (Mode == 1)
		for (auto S : P->Steps)
		{
			const FGuid Old = S.EditorGuid;
			S.EditorGuid = FGuid::NewGuid();
			Remap.Add(Old, S.EditorGuid);
			NewIds.Add(S.EditorGuid);
			S.StepId = Unique(TEXT("Step"), [&](FName Id)
			{
				return Data->Steps.ContainsByPredicate(
				           [&](const auto& V)
				{
					return V.StepId == Id;
				})
				    || Data->EditorDrafts.ContainsByPredicate([&](const auto& V)
				{
					return V.Step.StepId == Id;
				});
			});
			for (auto& A : S.Commands)
				A.Command = Duplicate(A.Command, Data);
			Data->Steps.Insert(S, Insert++);
		}
	if (Mode == 2)
	{
		Graph->ScopeCommand->Modify();
		for (UStageMemberCommand* C : P->Commands)
		{
			auto* Copy = Duplicate(C, Graph->ScopeCommand);
			ChildArray->Insert(Copy, Insert++);
			NewIds.Add(Copy->EditorGuid);
		}
	}
	if (Mode == 3)
		for (auto R : P->Reactions)
		{
			R.ReactionId = Unique(TEXT("Reaction"), [&](FName Id)
			{
				return Data->MemberReactions.ContainsByPredicate(
				           [&](const auto& V)
				{
					return V.ReactionId == Id;
				})
				    || Data->EditorDrafts.ContainsByPredicate([&](const auto& V)
				{
					return V.Reaction.ReactionId == Id;
				});
			});
			R.EditorGuid = FGuid::NewGuid();
			R.Command = Duplicate(R.Command, Data);
			for (auto& A : R.AdditionalPerformers)
				A.Command = Duplicate(A.Command, Data);
			Data->MemberReactions.Add(R);
			NewIds.Add(R.Command->EditorGuid);
		}
	if (Mode == 4)
		for (auto S : P->Scenes)
		{
			S.SceneId = Unique(TEXT("Scene"), [&](FName Id)
			{
				return Graph->Production->Scenes.ContainsByPredicate(
				           [&](const auto& V)
				{
					return V.SceneId == Id;
				})
				    || Graph->Production->EditorDrafts.ContainsByPredicate([&](const auto& V)
				{
					return V.Scene.SceneId == Id;
				});
			});
			const FGuid Old = S.EditorGuid, OldPrep = S.PreparationGuid;
			S.EditorGuid = FGuid::NewGuid();
			S.PreparationGuid = FGuid::NewGuid();
			Remap.Add(Old, S.EditorGuid);
			Remap.Add(OldPrep, S.PreparationGuid);
			NewIds.Add(S.EditorGuid);
			if (S.bPrepareBefore) NewIds.Add(S.PreparationGuid);
			Graph->Production->Scenes.Insert(S, Insert++);
		}
	if (Mode == 5)
		for (auto A : P->Assignments)
		{
			A.Command = Duplicate(A.Command, Data);
			AssignmentArray->Add(A);
			NewIds.Add(A.Command->EditorGuid);
		}
	for (const auto& G : P->Groups)
		Remap.Add(G.Id, FGuid::NewGuid());
	TSet<FGuid> Grouped;
	for (auto G : P->Groups)
	{
		G.Id = Remap[G.Id];
		const bool Internal = CommandRemap.Contains(G.Scope);
		G.Scope = Internal ? CommandRemap[G.Scope] : Graph->ScopeId;
		G.Parent = G.Parent.IsValid() ? Remap.FindRef(G.Parent) : Internal ? FGuid() : Graph->ActiveGroup;
		for (auto& Id : G.Members)
		{
			Id = Remap.FindRef(Id);
			Grouped.Add(Id);
		}
		G.Members.RemoveAll([](FGuid Id)
		{
			return !Id.IsValid();
		});
		Graph->Groups().Add(G);
		Graph->Positions().Add(G.Id, Location);
	}
	if (Graph->ActiveGroup.IsValid())
		if (auto* G = Graph->Groups().FindByPredicate([&](const auto& V)
		{
			return V.Id == Graph->ActiveGroup;
		}))
			for (FGuid Id : NewIds)
				if (!Grouped.Contains(Id)) G->Members.Add(Id);
	int32 I = 0;
	for (FGuid Id : NewIds)
		Graph->Positions().Add(Id, Location + FVector2D(I++ * 330, 120));
	if (bDisconnected)
	{
		auto Store = [&](FStageGraphDraft D)
		{
			D.Scope = Graph->ScopeId;
			D.Group = Graph->ActiveGroup;
			if (const auto* G = Graph->Groups().FindByPredicate([&](const auto& V)
			{
				return V.Scope == Graph->ScopeId && V.Members.Contains(D.Id);
			}))
				D.Group = G->Id;
			Data->EditorDrafts.Add(D);
		};
		if (Mode == 1)
			for (int32 N = Data->Steps.Num() - 1; N >= 0; --N)
				if (NewIds.Contains(Data->Steps[N].EditorGuid))
				{
					FStageGraphDraft D;
					D.Kind = EStageGraphDraftKind::Step;
					D.Id = Data->Steps[N].EditorGuid;
					D.Step = Data->Steps[N];
					Store(D);
					Data->Steps.RemoveAt(N);
				}
		if (Mode == 2)
			for (int32 N = ChildArray->Num() - 1; N >= 0; --N)
				if (NewIds.Contains(FStageScenarioGraphAdapter::Key((*ChildArray)[N])))
				{
					FStageGraphDraft D;
					D.Kind = EStageGraphDraftKind::Child;
					D.Id = FStageScenarioGraphAdapter::Key((*ChildArray)[N]);
					FStageMemberAssignment A;
					A.RoleId = Data->GetRoleOptions().IsEmpty() ? NAME_None : Data->GetRoleOptions()[0];
					A.Command = (*ChildArray)[N];
					D.Step.Commands.Add(A);
					Store(D);
					ChildArray->RemoveAt(N);
				}
		if (Mode == 3)
			for (int32 N = Data->MemberReactions.Num() - 1; N >= 0; --N)
				if (NewIds.Contains(FStageScenarioGraphAdapter::Key(Data->MemberReactions[N].Command)))
				{
					FStageGraphDraft D;
					D.Kind = EStageGraphDraftKind::Reaction;
					D.Id = Data->MemberReactions[N].EditorGuid;
					D.Reaction = Data->MemberReactions[N];
					Graph->Positions().Add(D.Id, Graph->Positions().FindRef(FStageScenarioGraphAdapter::Key(D.Reaction.Command)));
					Store(D);
					Data->MemberReactions.RemoveAt(N);
				}
		if (Mode == 4)
			for (int32 N = Graph->Production->Scenes.Num() - 1; N >= 0; --N)
				if (NewIds.Contains(Graph->Production->Scenes[N].EditorGuid))
				{
					FStageProductionDraft D;
					D.Id = Graph->Production->Scenes[N].EditorGuid;
					D.Scene = Graph->Production->Scenes[N];
					D.Scope = Graph->ScopeId;
					D.Group = Graph->ActiveGroup;
					if (const auto* G = Graph->Groups().FindByPredicate([&](const auto& V)
					{
						return V.Scope == Graph->ScopeId && V.Members.Contains(D.Id);
					}))
						D.Group = G->Id;
					Graph->Production->EditorDrafts.Add(D);
					Graph->Production->Scenes.RemoveAt(N);
				}
		if (Mode == 5)
			for (int32 N = AssignmentArray->Num() - 1; N >= 0; --N)
				if (NewIds.Contains(FStageScenarioGraphAdapter::Key((*AssignmentArray)[N].Command)))
				{
					FStageGraphDraft D;
					D.Kind = EStageGraphDraftKind::Assignment;
					D.Id = FStageScenarioGraphAdapter::Key((*AssignmentArray)[N].Command);
					D.Step.Commands.Add((*AssignmentArray)[N]);
					Store(D);
					AssignmentArray->RemoveAt(N);
				}
	}
	Graph->GetAsset()->MarkPackageDirty();
	Graph->GetAsset()->PostEditChange();
	return true;
}
