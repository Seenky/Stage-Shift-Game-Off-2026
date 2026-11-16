#include "StageScenarioGraphAdapter.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "Scenario/StageScenarioGraphSchema.h"
#include "Stage/Core/StageMemberCommand.h"
#include "Stage/Core/SequenceStageMemberCommand.h"
#include "Stage/Core/ParallelStageMemberCommand.h"
#include "Stage/Commands/StageMoveCommand.h"
#include "Stage/Commands/StageLookToCommand.h"
#include "Stage/Commands/StageDialogueCommand.h"
#include "Stage/Commands/StageWaitCommand.h"
#include "Components/Dialogue/StageDialogueTypes.h"
#include "ScopedTransaction.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"
#include "StageGraphOrganization.h"

namespace
{
	// Reject edits that introduce new definition errors, restoring the canonical arrays.
	struct FTopologyEdit
	{
		UStageDataAsset* Data;
		TArray<FStageStep> Steps;
		TArray<FStageMemberAssignment> Finale;
		TArray<FStageMemberReaction> Reactions;
		TMap<UStageMemberCommand*, TArray<TObjectPtr<UStageMemberCommand>>> ChildArrays;
		TArray<FString> Before;

		FTopologyEdit(UStageDataAsset* InData) : Data(InData), Steps(InData->Steps), Finale(InData->Finale), Reactions(InData->MemberReactions)
		{
			Data->ValidateDefinition(Before);
			TSet<UStageMemberCommand*> Seen;
			TFunction<void(UStageMemberCommand*)> Save = [&](UStageMemberCommand* Command)
			{
				if (!Command || Seen.Contains(Command)) return;
				Seen.Add(Command);
				if (Command->IsComposite()) ChildArrays.Add(Command, Command->GetChildCommands());
				for (UStageMemberCommand* Child : Command->GetChildCommands())
					Save(Child);
			};
			for (const auto& Step : Steps)
				for (const auto& Entry : Step.Commands)
					Save(Entry.Command);
			for (const auto& Entry : Finale)
				Save(Entry.Command);
			for (const auto& Rule : Reactions)
				for (const auto& A : Rule.GetAssignments())
					Save(A.Command);
		}

		bool Commit(FScopedTransaction& Transaction)
		{
			TArray<FString> After;
			Data->ValidateDefinition(After);
			if (After.ContainsByPredicate([&](const FString& Error)
			{
				const bool Structural = Error.Contains(TEXT("shared or cyclic")) || Error.Contains(TEXT("foreign Outer")) || Error.Contains(TEXT("RequiredSuccessRoleId"))
				    || Error.Contains(TEXT("unknown or duplicate role")) || Error.Contains(TEXT("duplicate reaction performer")) || Error.Contains(TEXT("duplicate StepId"))
				    || Error.Contains(TEXT("duplicate ReactionId")) || Error.Contains(TEXT("parallel branches conflict"));
				return Structural && !Before.Contains(Error);
			}))
			{
				Data->Steps = Steps;
				Data->Finale = Finale;
				Data->MemberReactions = Reactions;
				for (const auto& Pair : ChildArrays)
				{
					if (auto* Sequence = Cast<USequenceStageMemberCommand>(Pair.Key)) Sequence->Children = Pair.Value;
					if (auto* Parallel = Cast<UParallelStageMemberCommand>(Pair.Key)) Parallel->Children = Pair.Value;
				}
				Transaction.Cancel();
				UE_LOG(LogTemp, Warning, TEXT("Scenario edit rejected: %s"), *FString::Join(After, TEXT("; ")));
				return false;
			}
			Data->MarkPackageDirty();
			Data->PostEditChange();
			return true;
		}
	};

	FGuid StableKey(const FString& Text)
	{
		return FGuid(FCrc::StrCrc32(*Text), FCrc::StrCrc32(*(Text + TEXT("Scenario"))), 0, 1);
	}

	FName UniqueId(const TCHAR* Prefix, TFunctionRef<bool(FName)> Exists)
	{
		for (int32 Index = 1;; ++Index)
		{
			const FName Id(FString::Printf(TEXT("%s%d"), Prefix, Index));
			if (!Exists(Id)) return Id;
		}
	}
}

bool FStageScenarioGraphAdapter::Backup(UObject* Asset)
{
	if (!Asset) return false;
	static TSet<FString> BackedUp;
	const FString Source = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
	if (BackedUp.Contains(Source) || !IFileManager::Get().FileExists(*Source)) return true;
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("RefactorBackups") / (TEXT("ScenarioEditor_") + FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S_%s")));
	const FString Dest = Directory / FPaths::GetCleanFilename(Source);
	IFileManager::Get().MakeDirectory(*Directory, true);
	if (IFileManager::Get().Copy(*Dest, *Source) != COPY_OK) return false;
	const FString Manifest = FString::Printf(TEXT("# Scenario editor backup\n\nOriginal: `%s`\n\nBackup: `%s`\n\nMD5: `%s`\n"), *FPaths::ConvertRelativePathToFull(Source),
	    *FPaths::ConvertRelativePathToFull(Dest), *LexToString(FMD5Hash::HashFile(*Source)));
	if (!FFileHelper::SaveStringToFile(Manifest, *(Directory / TEXT("manifest.md")))) return false;
	BackedUp.Add(Source);
	return true;
}

FGuid FStageScenarioGraphAdapter::Key(const UObject* Object)
{
	if (const auto* Command = Cast<UStageMemberCommand>(Object); Command && Command->EditorGuid.IsValid()) return Command->EditorGuid;
	return StableKey(GetPathNameSafe(Object));
}

void FStageScenarioGraphAdapter::RenewGuids(UStageMemberCommand* Command)
{
	if (!Command) return;
	// Commands pasted out of the transient clipboard are persistent asset templates.
	Command->ClearFlags(RF_Transient);
	Command->SetFlags(RF_Transactional);
	Command->EditorGuid = FGuid::NewGuid();
	for (UStageMemberCommand* Child : Command->GetChildCommands())
		RenewGuids(Child);
}

void FStageScenarioGraphAdapter::Changed() const
{
	Data->MarkPackageDirty();
	Data->PostEditChange();
}

TArray<TObjectPtr<UStageMemberCommand>>* FStageScenarioGraphAdapter::Children(UStageMemberCommand* Parent) const
{
	if (auto* Sequence = Cast<USequenceStageMemberCommand>(Parent)) return &Sequence->Children;
	if (auto* Parallel = Cast<UParallelStageMemberCommand>(Parent)) return &Parallel->Children;
	return nullptr;
}

FStageMemberAssignment* FStageScenarioGraphAdapter::Assignment(const UStageScenarioEdNode* Node) const
{
	if (!Node) return nullptr;
	if (Node->IsDraft())
		return Data->EditorDrafts.IsValidIndex(Node->DraftIndex) && !Data->EditorDrafts[Node->DraftIndex].Step.Commands.IsEmpty()
		    ? &Data->EditorDrafts[Node->DraftIndex].Step.Commands[0]
		    : nullptr;
	if (Node->ReactionIndex != INDEX_NONE)
	{
		// Slate can still query the old node until the deferred graph refresh.
		if (!Data->MemberReactions.IsValidIndex(Node->ReactionIndex)) return nullptr;
		auto& R = Data->MemberReactions[Node->ReactionIndex];
		return Node->AssignmentIndex > 0 && R.AdditionalPerformers.IsValidIndex(Node->AssignmentIndex - 1) ? &R.AdditionalPerformers[Node->AssignmentIndex - 1] : nullptr;
	}
	auto* Assignments = Node->StepIndex == INDEX_NONE ? &Data->Finale : Data->Steps.IsValidIndex(Node->StepIndex) ? &Data->Steps[Node->StepIndex].Commands : nullptr;
	return Assignments && Assignments->IsValidIndex(Node->AssignmentIndex) ? &(*Assignments)[Node->AssignmentIndex] : nullptr;
}

TObjectPtr<UStageMemberCommand>* FStageScenarioGraphAdapter::CommandSlot(const UStageScenarioEdNode* Node) const
{
	if (!Node) return nullptr;
	if (Node->IsDraft() && Data->EditorDrafts.IsValidIndex(Node->DraftIndex))
	{
		auto& D = Data->EditorDrafts[Node->DraftIndex];
		if (D.Kind == EStageGraphDraftKind::Reaction) return &D.Reaction.Command;
		return D.Step.Commands.IsEmpty() ? nullptr : &D.Step.Commands[0].Command;
	}
	if (Node->ParentCommand)
	{
		auto* Array = Children(Node->ParentCommand);
		return Array && Array->IsValidIndex(Node->ChildIndex) ? &(*Array)[Node->ChildIndex] : nullptr;
	}
	if (Data->MemberReactions.IsValidIndex(Node->ReactionIndex))
	{
		auto& R = Data->MemberReactions[Node->ReactionIndex];
		return Node->AssignmentIndex > 0 && R.AdditionalPerformers.IsValidIndex(Node->AssignmentIndex - 1) ? &R.AdditionalPerformers[Node->AssignmentIndex - 1].Command
		                                                                                                   : &R.Command;
	}
	if (auto* Entry = Assignment(Node)) return &Entry->Command;
	return nullptr;
}

UStageMemberCommand* FStageScenarioGraphAdapter::NewCommand(UClass* Class, UObject* Outer) const
{
	if (!Class || !Class->IsChildOf(UStageMemberCommand::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
	auto* Command = NewObject<UStageMemberCommand>(Outer, Class, NAME_None, RF_Transactional);
	RenewGuids(Command);
	if (auto* Move = Cast<UStageMoveCommand>(Command); Move && Move->TargetId.IsNone()) Move->TargetId = Data->TargetIds.IsEmpty() ? NAME_None : Data->TargetIds[0];
	if (auto* Look = Cast<UStageLookToCommand>(Command); Look && Look->TargetId.IsNone()) Look->TargetId = Data->TargetIds.IsEmpty() ? NAME_None : Data->TargetIds[0];
	if (Command->IsA<UStageDialogueCommand>())
	{
		auto* Property = FindFProperty<FStructProperty>(Command->GetClass(), TEXT("CurrentLine"));
		auto* Line = Property->ContainerPtrToValuePtr<FDialogueLine>(Command);
		if (Line->Text.IsEmpty()) Line->Text = NSLOCTEXT("StageScenario", "NewLine", "New line");
	}
	return Command;
}

bool FStageScenarioGraphAdapter::AddStep(FName RoleId)
{
	if (!Data->GetRoleOptions().Contains(RoleId) || !Backup(Data)) return false;
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "AddStep", "Add stage step"));
	EnsureIdentities();
	FStageStep Step;
	Step.StepId = UniqueId(TEXT("Step"), [&](FName Id)
	{
		return Data->Steps.ContainsByPredicate([&](const auto& Item)
		{
			return Item.StepId == Id;
		});
	});
	FStageMemberAssignment Entry;
	Entry.RoleId = RoleId;
	Entry.Command = NewCommand(UStageWaitCommand::StaticClass(), Data);
	Step.EditorGuid = FGuid::NewGuid();
	Step.Commands.Add(Entry);
	Data->Steps.Add(Step);
	if (Data->Finale.IsEmpty())
	{
		Entry.Command = NewCommand(UStageWaitCommand::StaticClass(), Data);
		Data->Finale.Add(Entry);
	}
	return Edit.Commit(Transaction);
}

bool FStageScenarioGraphAdapter::AddReaction(FName RoleId)
{
	if (!Data->GetRoleOptions().Contains(RoleId) || !Backup(Data)) return false;
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "AddReaction", "Add stage reaction"));
	EnsureIdentities();
	FStageMemberReaction Rule;
	Rule.ReactionId = UniqueId(TEXT("Reaction"), [&](FName Id)
	{
		return Data->MemberReactions.ContainsByPredicate([&](const auto& Item)
		{
			return Item.ReactionId == Id;
		});
	});
	Rule.RoleId = RoleId;
	Rule.EventTag = FGameplayTag::RequestGameplayTag(TEXT("State.Member.Fallen"), false);
	Rule.Command = NewCommand(UStageDialogueCommand::StaticClass(), Data);
	Data->MemberReactions.Add(Rule);
	return Edit.Commit(Transaction);
}

bool FStageScenarioGraphAdapter::AddCommand(const UStageScenarioEdNode* Node, UClass* Class, FName RoleId, bool bChild)
{
	if (!Node || !Data->GetRoleOptions().Contains(RoleId)) return false;
	if (bChild)
	{
		auto* Array = Children(Node->Command);
		if (!Array) return false;
		auto* Candidate = NewCommand(Class, Node->Command);
		if (!Candidate) return false;
		if (Node->Command->IsA<UParallelStageMemberCommand>())
			for (const UStageMemberCommand* Child : *Array)
				if (Child && (Child->GetResourceMask() & Candidate->GetResourceMask())) return false;
		if (!Backup(Data)) return false;
		FTopologyEdit Edit(Data);
		FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "AddChild", "Add command child"));
		EnsureIdentities();
		Node->Command->Modify();
		Array->Add(Candidate);
		return Edit.Commit(Transaction);
	}
	if (Data->MemberReactions.IsValidIndex(Node->ReactionIndex))
	{
		auto& R = Data->MemberReactions[Node->ReactionIndex];
		if (R.GetAssignments().ContainsByPredicate([&](const auto& A)
		{
			return A.GetEffectiveRoleId() == RoleId;
		}))
			return false;
		auto* C = NewCommand(Class, Data);
		if (!C || !Backup(Data)) return false;
		FTopologyEdit Edit(Data);
		FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "ReactionPerformer", "Add reaction performer"));
		EnsureIdentities();
		FStageMemberAssignment A;
		A.RoleId = RoleId;
		A.Command = C;
		R.AdditionalPerformers.Add(A);
		return Edit.Commit(Transaction);
	}
	auto* Array = Node->StepIndex == INDEX_NONE ? &Data->Finale : Data->Steps.IsValidIndex(Node->StepIndex) ? &Data->Steps[Node->StepIndex].Commands : nullptr;
	if (!Array || Node->ReactionIndex != INDEX_NONE
	    || Array->ContainsByPredicate([&](const auto& Entry)
	{
		return Entry.GetEffectiveRoleId() == RoleId;
	}))
		return false;
	auto* Candidate = NewCommand(Class, Data);
	if (!Candidate || !Backup(Data)) return false;
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "AddAssignment", "Add parallel role action"));
	EnsureIdentities();
	FStageMemberAssignment Entry;
	Entry.RoleId = RoleId;
	Entry.Command = Candidate;
	Array->Add(Entry);
	return Edit.Commit(Transaction);
}

bool FStageScenarioGraphAdapter::ReplaceCommand(const UStageScenarioEdNode* Node, UClass* Class)
{
	auto* Slot = CommandSlot(Node);
	if (!Slot) return false;
	auto* Candidate = NewCommand(Class, Node->ParentCommand ? static_cast<UObject*>(Node->ParentCommand) : static_cast<UObject*>(Data));
	if (!Candidate) return false;
	if (Node->Command) Candidate->EditorGuid = Key(Node->Command);
	if (Node->ParentCommand && Node->ParentCommand->IsA<UParallelStageMemberCommand>())
		for (const UStageMemberCommand* Sibling : *Children(Node->ParentCommand))
			if (Sibling != Node->Command && Sibling && (Sibling->GetResourceMask() & Candidate->GetResourceMask())) return false;
	if (!Backup(Data)) return false;
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Replace", "Replace stage action"));
	EnsureIdentities();
	if (Node->ParentCommand) Node->ParentCommand->Modify();
	*Slot = Candidate;
	return Edit.Commit(Transaction);
}

bool FStageScenarioGraphAdapter::Delete(const UStageScenarioEdNode* Node)
{
	if (!Node || Node->Kind == EStageScenarioNodeKind::Start || Node->Kind == EStageScenarioNodeKind::Finale) return false;
	auto* AssignmentArray = Node->StepIndex == INDEX_NONE ? &Data->Finale : Data->Steps.IsValidIndex(Node->StepIndex) ? &Data->Steps[Node->StepIndex].Commands : nullptr;
	if (!Node->bStepRoot && Node->ReactionIndex == INDEX_NONE && !Node->ParentCommand && Node->Kind == EStageScenarioNodeKind::Command && (!AssignmentArray)) return false;
	if (!Backup(Data)) return false;
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Delete", "Delete stage item"));
	EnsureIdentities();
	if (Node->ParentCommand)
	{
		Node->ParentCommand->Modify();
		Children(Node->ParentCommand)->RemoveAt(Node->ChildIndex);
	}
	else if (Node->ReactionIndex != INDEX_NONE)
		Data->MemberReactions.RemoveAt(Node->ReactionIndex);
	else if (Node->Kind == EStageScenarioNodeKind::Step || Node->bStepRoot)
		Data->Steps.RemoveAt(Node->StepIndex);
	else
		AssignmentArray->RemoveAt(Node->AssignmentIndex);
	return Edit.Commit(Transaction);
}

bool FStageScenarioGraphAdapter::Duplicate(const UStageScenarioEdNode* Node)
{
	if (!Node || Node->Kind == EStageScenarioNodeKind::Start || Node->Kind == EStageScenarioNodeKind::Finale || !Backup(Data)) return false;
	if (Node->ParentCommand && Node->ParentCommand->IsA<UParallelStageMemberCommand>() && Node->Command->GetResourceMask()) return false;
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Duplicate", "Duplicate stage item"));
	EnsureIdentities();
	if (Node->ParentCommand)
	{
		Node->ParentCommand->Modify();
		auto* Copy = DuplicateObject<UStageMemberCommand>(Node->Command, Node->ParentCommand);
		RenewGuids(Copy);
		Children(Node->ParentCommand)->Insert(Copy, Node->ChildIndex + 1);
	}
	else if (Node->ReactionIndex != INDEX_NONE)
	{
		auto Copy = Data->MemberReactions[Node->ReactionIndex];
		Copy.ReactionId = UniqueId(TEXT("Reaction"), [&](FName Id)
		{
			return Data->MemberReactions.ContainsByPredicate([&](const auto& Item)
			{
				return Item.ReactionId == Id;
			});
		});
		Copy.Command = DuplicateObject<UStageMemberCommand>(Copy.Command, Data);
		RenewGuids(Copy.Command);
		Data->MemberReactions.Insert(Copy, Node->ReactionIndex + 1);
	}
	else
	{
		FStageStep Copy;
		Copy.EditorGuid = FGuid::NewGuid();
		Copy.StepId = UniqueId(TEXT("Step"), [&](FName Id)
		{
			return Data->Steps.ContainsByPredicate([&](const auto& Item)
			{
				return Item.StepId == Id;
			});
		});
		if (Node->Kind == EStageScenarioNodeKind::Step || Node->bStepRoot)
			Copy.Commands = Data->Steps[Node->StepIndex].Commands;
		else if (auto* Entry = Assignment(Node))
			Copy.Commands.Add(*Entry);
		for (auto& Entry : Copy.Commands)
		{
			Entry.Command = DuplicateObject<UStageMemberCommand>(Entry.Command, Data);
			RenewGuids(Entry.Command);
		}
		Data->Steps.Insert(Copy, Node->StepIndex == INDEX_NONE ? Data->Steps.Num() : Node->StepIndex + 1);
	}
	return Edit.Commit(Transaction);
}

bool FStageScenarioGraphAdapter::Reorder(const UStageScenarioEdNode* Node, int32 Delta)
{
	if (!Node) return false;
	const int32 Index = Node->ParentCommand ? Node->ChildIndex : Node->ReactionIndex != INDEX_NONE ? Node->ReactionIndex : Node->StepIndex;
	const int32 Num = Node->ParentCommand ? Children(Node->ParentCommand)->Num() : Node->ReactionIndex != INDEX_NONE ? Data->MemberReactions.Num() : Data->Steps.Num();
	if (Index < 0 || Index + Delta < 0 || Index + Delta >= Num || !Backup(Data)) return false;
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Reorder", "Reorder stage item"));
	EnsureIdentities();
	if (Node->ParentCommand)
	{
		Node->ParentCommand->Modify();
		Children(Node->ParentCommand)->Swap(Index, Index + Delta);
	}
	else if (Node->ReactionIndex != INDEX_NONE)
		Data->MemberReactions.Swap(Index, Index + Delta);
	else
		Data->Steps.Swap(Index, Index + Delta);
	return Edit.Commit(Transaction);
}

bool FStageScenarioGraphAdapter::MoveStepAfter(int32 Index, int32 After)
{
	if (!Data->Steps.IsValidIndex(Index) || !Data->Steps.IsValidIndex(After) || Index == After || !Backup(Data)) return false;
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Connect", "Reorder linear stage steps"));
	EnsureIdentities();
	auto Step = Data->Steps[Index];
	Data->Steps.RemoveAt(Index);
	if (Index < After) --After;
	Data->Steps.Insert(Step, After + 1);
	return Edit.Commit(Transaction);
}

bool FStageScenarioGraphAdapter::SetRole(const UStageScenarioEdNode* Node, FName RoleId, bool bSubject)
{
	if (!Node || (!Data->GetRoleOptions().Contains(RoleId) && !(bSubject && RoleId.IsNone())) || !Backup(Data)) return false;
	if (Node->ReactionIndex == INDEX_NONE)
	{
		auto* Entry = Assignment(Node);
		if (!Entry || bSubject) return false;
		const auto& Assignments = Node->StepIndex == INDEX_NONE ? Data->Finale : Data->Steps[Node->StepIndex].Commands;
		for (const auto& Other : Assignments)
			if (&Other != Entry && Other.GetEffectiveRoleId() == RoleId) return false;
	}
	FTopologyEdit Edit(Data);
	FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "Role", "Set local role"));
	EnsureIdentities();
	if (Node->ReactionIndex != INDEX_NONE)
	{
		auto& Rule = Data->MemberReactions[Node->ReactionIndex];
		if (bSubject)
		{
			Rule.SubjectRoleId = RoleId;
			Rule.SubjectRoleTag = {};
		}
		else if (Node->AssignmentIndex > 0 && Rule.AdditionalPerformers.IsValidIndex(Node->AssignmentIndex - 1))
			Rule.AdditionalPerformers[Node->AssignmentIndex - 1].RoleId = RoleId;
		else
			Rule.RoleId = RoleId;
	}
	else
		Assignment(Node)->RoleId = RoleId;
	return Edit.Commit(Transaction);
}

void FStageScenarioGraphAdapter::Build(UStageScenarioEdGraph* Graph, bool bReactions) const
{
	Graph->bReactions = bReactions;
	BuildView(Graph);
}
