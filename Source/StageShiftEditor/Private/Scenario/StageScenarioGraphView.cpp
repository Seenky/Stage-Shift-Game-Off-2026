#include "StageScenarioGraphAdapter.h"
#include "StageGraphOrganization.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "Scenario/StageScenarioGraphSchema.h"
#include "Stage/Core/StageMemberCommand.h"
#include "Stage/Core/SequenceStageMemberCommand.h"
#include "Stage/Core/ParallelStageMemberCommand.h"
#include "Stage/Commands/StageWaitCommand.h"
#include "Stage/Commands/StageMoveCommand.h"
#include "Stage/Commands/StageLookToCommand.h"
#include "Stage/Commands/StageDialogueCommand.h"
#include "ScopedTransaction.h"
#include "StageCommandCatalog.h"
#include "StageGraphDrafts.h"
#include "Stage/Core/StageCondition.h"
#include "UObject/UnrealType.h"

namespace
{
	FGuid ViewKey(const FString& Text)
	{
		return FGuid(FCrc::StrCrc32(*Text), FCrc::StrCrc32(*(Text + TEXT("Scenario"))), 0, 1);
	}

	FName NextStepId(const UStageDataAsset* Data)
	{
		for (int32 I = 1;; ++I)
		{
			const FName Id(*FString::Printf(TEXT("Step%d"), I));
			if (!Data->Steps.ContainsByPredicate([&](const auto& S)
			{
				return S.StepId == Id;
			}))
				return Id;
		}
	}
}

FGuid FStageScenarioGraphAdapter::StepKey(const UStageDataAsset* Asset, int32 Index)
{
	if (!Asset->Steps.IsValidIndex(Index)) return FGuid();
	const auto& Step = Asset->Steps[Index];
	if (Step.EditorGuid.IsValid()) return Step.EditorGuid;
	if (Step.Commands.Num() == 1 && Step.Commands[0].Command) return Key(Step.Commands[0].Command);
	return ViewKey(Asset->GetPathName() + TEXT("Step") + Step.StepId.ToString() + FString::FromInt(Index));
}

FGuid FStageScenarioGraphAdapter::ReactionKey(const UStageDataAsset* Asset, int32 Index)
{
	if (!Asset->MemberReactions.IsValidIndex(Index)) return FGuid();
	const auto& R = Asset->MemberReactions[Index];
	return R.EditorGuid.IsValid() ? R.EditorGuid : Key(R.Command);
}

void FStageScenarioGraphAdapter::EnsureIdentities()
{
	Data->SetFlags(RF_Transactional);
	Data->Modify();
	for (int32 I = 0; I < Data->Steps.Num(); ++I)
		if (!Data->Steps[I].EditorGuid.IsValid()) Data->Steps[I].EditorGuid = StepKey(Data, I);
	TSet<UStageMemberCommand*> Seen;
	TFunction<void(UStageMemberCommand*)> Visit = [&](UStageMemberCommand* C)
	{
		if (!C || Seen.Contains(C)) return;
		Seen.Add(C);
		C->SetFlags(RF_Transactional);
		if (!C->EditorGuid.IsValid())
		{
			C->Modify();
			C->EditorGuid = Key(C);
		}
		for (UStageMemberCommand* Child : C->GetChildCommands())
			Visit(Child);
	};
	for (auto& Step : Data->Steps)
		for (auto& A : Step.Commands)
			Visit(A.Command);
	for (auto& A : Data->Finale)
		Visit(A.Command);
	for (int32 I = 0; I < Data->MemberReactions.Num(); ++I)
	{
		auto& R = Data->MemberReactions[I];
		if (!R.EditorGuid.IsValid()) R.EditorGuid = ReactionKey(Data, I);
		for (const auto& A : R.GetAssignments())
			Visit(A.Command);
	}
	for (auto& D : Data->EditorDrafts)
		Visit(D.GetCommand());
}

bool FStageScenarioGraphAdapter::AddTypedStep(UClass* Class, FName RoleId, int32 After)
{
	if (!Data->GetRoleOptions().Contains(RoleId) || !Backup(Data)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "TypedStep", "Add scenario action"));
	EnsureIdentities();
	auto* Command = NewCommand(Class, Data);
	if (!Command) return false;
	FStageStep Step;
	Step.StepId = NextStepId(Data);
	Step.EditorGuid = FGuid::NewGuid();
	FStageMemberAssignment A;
	A.RoleId = RoleId;
	A.Command = Command;
	Step.Commands.Add(A);
	Data->Steps.Insert(Step, After == INDEX_NONE ? Data->Steps.Num() : FMath::Clamp(After + 1, 0, Data->Steps.Num()));
	if (Data->Finale.IsEmpty())
	{
		A.Command = NewCommand(UStageWaitCommand::StaticClass(), Data);
		auto* P = FindFProperty<FFloatProperty>(A.Command->GetClass(), TEXT("WaitTime"));
		P->SetPropertyValue_InContainer(A.Command, 0);
		Data->Finale.Add(A);
	}
	Changed();
	return true;
}

bool FStageScenarioGraphAdapter::AddTypedReaction(UClass* Class, FName RoleId)
{
	if (!Data->GetRoleOptions().Contains(RoleId) || !Backup(Data)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "TypedReaction", "Add reaction"));
	EnsureIdentities();
	auto* C = NewCommand(Class, Data);
	if (!C) return false;
	FStageMemberReaction R;
	for (int32 I = 1;; ++I)
	{
		R.ReactionId = FName(*FString::Printf(TEXT("Reaction%d"), I));
		if (!Data->MemberReactions.ContainsByPredicate([&](const auto& Item)
		{
			return Item.ReactionId == R.ReactionId;
		}))
			break;
	}
	R.RoleId = RoleId;
	R.Command = C;
	R.EventTag = FGameplayTag::RequestGameplayTag(TEXT("State.Member.Fallen"), false);
	Data->MemberReactions.Add(R);
	Changed();
	return true;
}

bool FStageScenarioGraphAdapter::AddInView(UStageScenarioEdGraph* Graph, UClass* Class, const UStageScenarioEdNode* From, FVector2D Position)
{
	return FStageGraphDrafts::Add(Graph, Class, From, Position);
}

bool FStageScenarioGraphAdapter::Wrap(const UStageScenarioEdNode* Node, UClass* Container)
{
	auto* Slot = CommandSlot(Node);
	if (!Slot || !*Slot || !Backup(Data)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Wrap", "Wrap command"));
	EnsureIdentities();
	auto* Wrapper = NewCommand(Container, Node->ParentCommand ? static_cast<UObject*>(Node->ParentCommand) : Data);
	auto* Array = Children(Wrapper);
	if (!Array) return false;
	if (Node->ParentCommand) Node->ParentCommand->Modify();
	auto* Inner = DuplicateObject<UStageMemberCommand>(*Slot, Wrapper);
	RenewGuids(Inner);
	Wrapper->EditorGuid = Key(*Slot);
	Array->Add(Inner);
	*Slot = Wrapper;
	Changed();
	return true;
}

bool FStageScenarioGraphAdapter::SetContainerMode(const UStageScenarioEdNode* Node, UClass* Class)
{
	auto* Slot = CommandSlot(Node);
	if (!Slot || !*Slot || !Backup(Data)) return false;
	if (!(*Slot)->IsComposite()) return Wrap(Node, Class);
	if ((*Slot)->GetClass() == Class) return true;
	auto* Replacement = NewCommand(Class, (*Slot)->GetOuter());
	auto* Array = Children(Replacement);
	if (!Array) return false;
	uint8 Used = 0;
	for (UStageMemberCommand* Child : (*Slot)->GetChildCommands())
	{
		if (Class->IsChildOf(UParallelStageMemberCommand::StaticClass()) && (Used & Child->GetResourceMask())) return false;
		Used |= Child->GetResourceMask();
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "ContainerMode", "Change action execution mode"));
	EnsureIdentities();
	if (Node->ParentCommand) Node->ParentCommand->Modify();
	Replacement->EditorGuid = Key(*Slot);
	for (UStageMemberCommand* Child : (*Slot)->GetChildCommands())
		Array->Add(DuplicateObject<UStageMemberCommand>(Child, Replacement));
	for (auto& Draft : Data->EditorDrafts)
		if (Draft.Kind == EStageGraphDraftKind::Child && Draft.Scope == Key(*Slot) && !Draft.Step.Commands.IsEmpty())
			Draft.Step.Commands[0].Command = DuplicateObject<UStageMemberCommand>(Draft.GetCommand(), Replacement);
	*Slot = Replacement;
	Changed();
	return true;
}

bool FStageScenarioGraphAdapter::SetTarget(const UStageScenarioEdNode* Node, FName Id)
{
	if (!Node || !Node->Command || !Data->TargetIds.Contains(Id) || !Backup(Data)) return false;
	if (!Node->Command->IsA<UStageMoveCommand>() && !Node->Command->IsA<UStageLookToCommand>()) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Target", "Set target"));
	Data->Modify();
	Node->Command->Modify();
	if (auto* Move = Cast<UStageMoveCommand>(Node->Command)) Move->TargetIds = {Id};
	if (auto* Look = Cast<UStageLookToCommand>(Node->Command)) Look->TargetId = Id;
	Changed();
	return true;
}

bool FStageScenarioGraphAdapter::MoveElementsAfter(const UStageScenarioEdGraph* Graph, const UStageScenarioEdNode* Node, const UStageScenarioEdNode* After)
{
	if (Graph->IsStorySteps() && Data->bUseStepLinks) return false;
	if (!Graph->bLinear || !Node || !After || Node == After || Node->Kind == EStageScenarioNodeKind::Finale || After->Kind == EStageScenarioNodeKind::Finale || !Backup(Data))
		return false;
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Order", "Reorder execution"));
	EnsureIdentities();
	if (Graph->ScopeCommand)
	{
		auto* Array = Children(Graph->ScopeCommand);
		if (!Array) return false;
		Graph->ScopeCommand->Modify();
		int32 ScopeStart = 0;
		if (Graph->ActiveGroup.IsValid())
			for (int32 I = 0; I < Array->Num(); ++I)
				if (Graph->Nodes.ContainsByPredicate([&](const UEdGraphNode* N)
				{
					return CastChecked<UStageScenarioEdNode>(N)->RepresentedIds.Contains(Key((*Array)[I]));
				}))
				{
					ScopeStart = I;
					break;
				}
		TArray<TObjectPtr<UStageMemberCommand>> Moving;
		Array->RemoveAll([&](const auto& C)
		{
			if (Node->RepresentedIds.Contains(Key(C)))
			{
				Moving.Add(C);
				return true;
			}
			return false;
		});
		int32 Index = ScopeStart;
		if (After->Kind != EStageScenarioNodeKind::Start)
			for (int32 I = 0; I < Array->Num(); ++I)
				if (After->RepresentedIds.Contains(Key((*Array)[I]))) Index = I + 1;
		if (Moving.IsEmpty())
		{
			Transaction.Cancel();
			return false;
		}
		for (int32 I = 0; I < Moving.Num(); ++I)
			Array->Insert(Moving[I], Index + I);
	}
	else
	{
		TArray<FStageStep> Before = Data->Steps, Moving;
		int32 ScopeStart = 0;
		if (Graph->ActiveGroup.IsValid())
			for (int32 I = 0; I < Data->Steps.Num(); ++I)
				if (Graph->Nodes.ContainsByPredicate([&](const UEdGraphNode* N)
				{
					return CastChecked<UStageScenarioEdNode>(N)->RepresentedIds.Contains(Data->Steps[I].EditorGuid);
				}))
				{
					ScopeStart = I;
					break;
				}
		Data->Steps.RemoveAll([&](const auto& S)
		{
			if (Node->RepresentedIds.Contains(S.EditorGuid))
			{
				Moving.Add(S);
				return true;
			}
			return false;
		});
		if (Moving.IsEmpty())
		{
			Transaction.Cancel();
			return false;
		}
		int32 Index = ScopeStart;
		if (After->Kind != EStageScenarioNodeKind::Start)
			for (int32 I = 0; I < Data->Steps.Num(); ++I)
				if (After->RepresentedIds.Contains(Data->Steps[I].EditorGuid)) Index = I + 1;
		for (int32 I = 0; I < Moving.Num(); ++I)
			Data->Steps.Insert(Moving[I], Index + I);
		TArray<FString> Errors;
		Data->ValidateDefinition(Errors);
		if (Errors.ContainsByPredicate([](const FString& E)
		{
			return E.Contains(TEXT("RequiredSuccessRoleId"));
		}))
		{
			Data->Steps = Before;
			Transaction.Cancel();
			return false;
		}
	}
	Changed();
	return true;
}

void FStageScenarioGraphAdapter::EnableStepLinks()
{
	if (Data->bUseStepLinks) return;
	for (FStageStep& Step : Data->Steps)
		if (Step.StepId.IsNone()) Step.StepId = NextStepId(Data);
	for (int32 Index = 0; Index < Data->Steps.Num(); ++Index)
	{
		auto& Step = Data->Steps[Index];
		Step.ThenStepId = Data->Steps.IsValidIndex(Index + 1) ? Data->Steps[Index + 1].StepId : NAME_None;
		Step.FailedStepId = NAME_None;
	}
	Data->EntryStepId = Data->Steps.IsEmpty() ? NAME_None : Data->Steps[0].StepId;
	Data->bUseStepLinks = true;
}

bool FStageScenarioGraphAdapter::ConnectStep(const UStageScenarioEdNode* To, const UStageScenarioEdNode* From, bool bOnFailed)
{
	if (!From || !Backup(Data)) return false;
	const bool bEntry = From->Kind == EStageScenarioNodeKind::Start;
	if ((!bEntry && !Data->Steps.IsValidIndex(From->StepIndex)) || (bEntry && (!To || bOnFailed))) return false;
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "LinkStep", "Connect Stage step"));
	EnsureIdentities();
	const TArray<FStageStep> Before = Data->Steps;
	const FName BeforeEntry = Data->EntryStepId;
	const bool bBeforeLinks = Data->bUseStepLinks;
	EnableStepLinks();
	const FName Next = To && Data->Steps.IsValidIndex(To->StepIndex) ? Data->Steps[To->StepIndex].StepId : NAME_None;
	if (bEntry)
		Data->EntryStepId = Next;
	else if (bOnFailed)
		Data->Steps[From->StepIndex].FailedStepId = Next;
	else
		Data->Steps[From->StepIndex].ThenStepId = Next;
	TArray<FString> Errors;
	if (!Data->ValidateStepLinks(Errors))
	{
		Data->Steps = Before;
		Data->EntryStepId = BeforeEntry;
		Data->bUseStepLinks = bBeforeLinks;
		Transaction.Cancel();
		return false;
	}
	Changed();
	return true;
}

void FStageScenarioGraphAdapter::BuildView(UStageScenarioEdGraph* Graph) const
{
	Graph->Nodes.Reset();
	Graph->Scenario = Data;
	Graph->Schema = UStageScenarioGraphSchema::StaticClass();
	Graph->ScopeId = Graph->ScopeCommand ? Key(Graph->ScopeCommand)
	    : Graph->bReactionAssignments    ? ReactionKey(Data, Graph->ScopeReaction)
	    : Graph->bParallelAssignments    ? (Graph->ScopeStep == INDEX_NONE ? ViewKey(TEXT("Finale")) : StepKey(Data, Graph->ScopeStep))
	                                     : ViewKey(Graph->bReactions ? TEXT("Reactions") : TEXT("Story"));
	TArray<UStageScenarioEdNode*> Logical;
	auto Make = [&](EStageScenarioNodeKind Kind, FGuid Id)
	{
		auto* N = NewObject<UStageScenarioEdNode>(Graph, NAME_None, RF_Transactional);
		N->Kind = Kind;
		N->NodeGuid = Id;
		N->bCanRenameNode = true;
		N->AllocateDefaultPins();
		return N;
	};
	auto Describe = [&](UStageScenarioEdNode* N, UStageMemberCommand* C)
	{
		N->Command = C;
		N->Title = C ? FStageCommandCatalog::Name(C->GetClass()) : FText::FromString(TEXT("Missing command"));
		if (C && C->IsComposite())
			N->Summary = FString::Printf(TEXT("%d actions · double-click to open"), C->GetChildCommands().Num());
		else if (const auto* Move = Cast<UStageMoveCommand>(C))
		{
			const TConstArrayView<FName> Targets = Move->GetMoveTargets();
			if (Targets.Num() == 1) N->Summary = Targets[0].ToString();
			else if (Targets.Num() > 1) N->Summary = FString::Printf(TEXT("Random (%d targets)"), Targets.Num());
		}
		else if (const auto* Look = Cast<UStageLookToCommand>(C))
			N->Summary = Look->TargetId.ToString();
		else if (C && C->IsA<UStageDialogueCommand>())
		{
			const auto* P = FindFProperty<FStructProperty>(C->GetClass(), TEXT("CurrentLine"));
			N->Summary = P->ContainerPtrToValuePtr<FDialogueLine>(C)->Text.ToString().Left(100);
		}
	};
	if (Graph->ScopeCommand)
	{
		Graph->bLinear = !Graph->ScopeCommand->IsA<UParallelStageMemberCommand>();
		int32 Step = INDEX_NONE, AssignmentIndex = INDEX_NONE, Reaction = INDEX_NONE;
		TSet<const UStageMemberCommand*> Seen;
		TFunction<bool(const UStageMemberCommand*)> Contains = [&](const UStageMemberCommand* C)
		{
			if (C == Graph->ScopeCommand) return true;
			if (!C || Seen.Contains(C)) return false;
			Seen.Add(C);
			for (const UStageMemberCommand* Child : C->GetChildCommands())
				if (Contains(Child)) return true;
			return false;
		};
		bool Found = false;
		for (int32 I = 0; I < Data->Steps.Num() && !Found; ++I)
			for (int32 J = 0; J < Data->Steps[I].Commands.Num(); ++J)
				if (Contains(Data->Steps[I].Commands[J].Command))
				{
					Step = I;
					AssignmentIndex = J;
					Found = true;
					break;
				}
		if (!Found)
			for (int32 J = 0; J < Data->Finale.Num(); ++J)
				if (Contains(Data->Finale[J].Command))
				{
					AssignmentIndex = J;
					Found = true;
					break;
				}
		if (!Found)
			for (int32 J = 0; J < Data->MemberReactions.Num() && !Found; ++J)
			{
				const auto Entries = Data->MemberReactions[J].GetAssignments();
				for (int32 K = 0; K < Entries.Num(); ++K)
					if (Contains(Entries[K].Command))
					{
						Reaction = J;
						AssignmentIndex = K;
						Found = true;
						break;
					}
			}
		int32 I = 0;
		for (UStageMemberCommand* C : Graph->ScopeCommand->GetChildCommands())
		{
			auto* N = Make(EStageScenarioNodeKind::Command, Key(C));
			N->ParentCommand = Graph->ScopeCommand;
			N->ChildIndex = I++;
			N->StepIndex = Step;
			N->AssignmentIndex = AssignmentIndex;
			N->ReactionIndex = Reaction;
			Describe(N, C);
			Logical.Add(N);
		}
	}
	else if (Graph->bParallelAssignments || Graph->bReactionAssignments)
	{
		Graph->bLinear = false;
		const auto Entries = Graph->bReactionAssignments ? Data->MemberReactions[Graph->ScopeReaction].GetAssignments()
		    : Graph->ScopeStep == INDEX_NONE             ? Data->Finale
		                                                 : Data->Steps[Graph->ScopeStep].Commands;
		for (int32 I = 0; I < Entries.Num(); ++I)
		{
			auto* N = Make(EStageScenarioNodeKind::Command, Key(Entries[I].Command));
			N->StepIndex = Graph->ScopeStep;
			N->ReactionIndex = Graph->bReactionAssignments ? Graph->ScopeReaction : INDEX_NONE;
			N->AssignmentIndex = I;
			Describe(N, Entries[I].Command);
			Logical.Add(N);
		}
	}
	else if (Graph->bReactions)
	{
		Graph->bLinear = false;
		for (int32 I = 0; I < Data->MemberReactions.Num(); ++I)
		{
			const auto& R = Data->MemberReactions[I];
			auto* Event = Make(EStageScenarioNodeKind::Reaction, FStageGraphDrafts::EventId(ReactionKey(Data, I)));
			Event->ReactionIndex = I;
			Event->Title = FText::FromString(R.EventTag.IsValid() ? R.EventTag.ToString() : TEXT("Choose event"));
			Event->Summary = R.ReactionId.ToString() + FString::Printf(TEXT(" · %d performers · double-click to edit"), R.GetAssignments().Num());
			Event->NodePosX = I * 650;
			Event->NodePosY = 0;
			Graph->AddNode(Event);
			const auto* EventPosition = Graph->Positions().Find(Event->NodeGuid);
			if (!EventPosition) EventPosition = Graph->Positions().Find(ViewKey(Data->GetPathName() + R.ReactionId.ToString() + FString::FromInt(I)));
			if (EventPosition)
			{
				Event->NodePosX = EventPosition->X;
				Event->NodePosY = EventPosition->Y;
			}
			const auto Entries = R.GetAssignments();
			for (int32 J = 0; J < Entries.Num(); ++J)
			{
				auto* Action = Make(EStageScenarioNodeKind::Command, Key(Entries[J].Command));
				Action->ReactionIndex = I;
				Action->AssignmentIndex = J;
				Describe(Action, Entries[J].Command);
				Action->NodePosX = I * 650 + 300;
				Action->NodePosY = J * 170;
				Graph->AddNode(Action);
				Event->FindPin(TEXT("Execute"), EGPD_Output)->MakeLinkTo(Action->FindPin(TEXT("Execute"), EGPD_Input));
				if (const auto* P = Graph->Positions().Find(Action->NodeGuid))
				{
					Action->NodePosX = P->X;
					Action->NodePosY = P->Y;
				}
			}
		}
		FStageGraphDrafts::Project(Graph);
		Graph->NotifyGraphChanged();
		return;
	}
	else
	{
		Graph->bLinear = true;
		for (int32 I = 0; I < Data->Steps.Num(); ++I)
		{
			const auto& Step = Data->Steps[I];
			const bool Single = Step.Commands.Num() == 1;
			auto* N = Make(Step.Branch ? EStageScenarioNodeKind::Branch : Single ? EStageScenarioNodeKind::Command : EStageScenarioNodeKind::Step, StepKey(Data, I));
			N->StepIndex = I;
			N->bStepRoot = true;
			if (!Step.Branch) N->CreatePin(EGPD_Output, TEXT("StageOrder"), UStageScenarioEdNode::FailedPin);
			if (Step.Branch)
			{
				N->Title = FText::FromString(TEXT("Branch"));
				if (const auto* Condition = Step.Branch->Condition.Get()) N->Summary = Condition->GetClass()->GetDisplayNameText().ToString();
			}
			else if (Single)
			{
				N->AssignmentIndex = 0;
				Describe(N, Step.Commands[0].Command);
			}
			else
			{
				N->Title = FText::FromString(TEXT("Parallel"));
				N->Summary = FString::Printf(TEXT("%d performers · all completed"), Step.Commands.Num());
				N->bParallel = true;
			}
			// Slate edits the first title line as StepId; keep type/order visible on the second.
			N->Title = FText::FromString(FString::Printf(TEXT("%s\n%d · %s"), *Step.StepId.ToString(), I + 1, *N->Title.ToString()));
			Logical.Add(N);
		}
		auto* Finale = Make(EStageScenarioNodeKind::Finale, ViewKey(TEXT("Finale")));
		Finale->Title = FText::FromString(TEXT("Finale"));
		Finale->Summary = FString::Printf(TEXT("%d actions · double-click to open"), Data->Finale.Num());
		Logical.Add(Finale);
	}
	FStageGraphOrganization::Project(Graph, Logical);
	FStageGraphDrafts::Project(Graph);
	Graph->NotifyGraphChanged();
}
