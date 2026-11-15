#include "StageGraphParameters.h"
#include "StageScenarioGraphAdapter.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/Data/StageCommandParameters.h"
#include "Stage/Core/StageMemberCommand.h"
#include "PropertyHandle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

namespace
{
	void Visit(UStageDataAsset* Data, TFunctionRef<void(UStageMemberCommand*)> Callback)
	{
		TSet<UStageMemberCommand*> Seen;
		TFunction<void(UStageMemberCommand*)> Walk = [&](UStageMemberCommand* C)
		{
			if (!C || Seen.Contains(C)) return;
			Seen.Add(C);
			Callback(C);
			for (UStageMemberCommand* Child : C->GetChildCommands())
				Walk(Child);
		};
		for (auto& S : Data->Steps)
			for (auto& A : S.Commands)
				Walk(A.Command);
		for (auto& A : Data->Finale)
			Walk(A.Command);
		for (auto& R : Data->MemberReactions)
			for (const auto& A : R.GetAssignments())
				Walk(A.Command);
		for (auto& D : Data->EditorDrafts)
			Walk(D.GetCommand());
	}

	bool Property(UStageDataAsset* Data, UStageMemberCommand* Command, FName Path, FProperty*& P, void*& Address)
	{
		if (!Data || !Command || !Command->IsIn(Data)) return false;
		bool Reachable = false;
		Visit(Data, [&](UStageMemberCommand* C)
		{
			Reachable |= C == Command;
		});
		return Reachable && FStageCommandParameters::Resolve(Command, Path, P, Address) && P->HasAnyPropertyFlags(CPF_Edit) && FStageCommandParameters::Supports(P);
	}

	void Changed(UStageDataAsset* Data, UStageMemberCommand* Command)
	{
		Data->MarkPackageDirty();
		Command->PostEditChange();
		Data->PostEditChange();
	}
}

bool FStageGraphParameters::Promote(UStageDataAsset* Data, UStageMemberCommand* Command, FName Path, FName Name)
{
	FProperty* P = nullptr;
	void* Address = nullptr;
	if (!Data || Name.IsNone() || Data->Parameters.FindPropertyDescByName(Name) || !Property(Data, Command, Path, P, Address) || !FStageScenarioGraphAdapter::Backup(Data))
		return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "PromoteParameter", "Promote to graph parameter"));
	Data->Modify();
	Command->Modify();
	Data->Parameters.AddProperty(Name, P);
	const auto* Desc = Data->Parameters.FindPropertyDescByName(Name);
	if (!Desc || !Desc->CachedProperty || !P->SameType(Desc->CachedProperty)) return false;
	Desc->CachedProperty->CopyCompleteValue(Desc->CachedProperty->ContainerPtrToValuePtr<void>(Data->Parameters.GetMutableValue().GetMemory()), Address);
	Command->GraphParameterBindings.Add(Path, Desc->ID);
	Changed(Data, Command);
	return true;
}

bool FStageGraphParameters::Bind(UStageDataAsset* Data, UStageMemberCommand* Command, FName Path, FGuid Id)
{
	FProperty* P = nullptr;
	void* Address = nullptr;
	if (!Property(Data, Command, Path, P, Address) || !FStageScenarioGraphAdapter::Backup(Data)) return false;
	const auto* Desc = Data->Parameters.FindPropertyDescByID(Id);
	if (!Desc || !Desc->CachedProperty || !P->SameType(Desc->CachedProperty)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "BindParameter", "Bind graph parameter"));
	Data->Modify();
	Command->Modify();
	Command->GraphParameterBindings.Add(Path, Id);
	TArray<FString> Errors;
	FStageCommandParameters::ApplyOne(Command, Data->Parameters, Errors);
	Changed(Data, Command);
	return true;
}

bool FStageGraphParameters::Unbind(UStageDataAsset* Data, UStageMemberCommand* Command, FName Path)
{
	if (!Data || !Command || !Command->IsIn(Data) || !Command->GraphParameterBindings.Contains(Path) || !FStageScenarioGraphAdapter::Backup(Data)) return false;
	const FScopedTransaction Transaction(NSLOCTEXT("StageScenario", "UnbindParameter", "Use local action value"));
	Data->Modify();
	Command->Modify();
	Command->GraphParameterBindings.Remove(Path);
	Changed(Data, Command);
	return true;
}

void FStageGraphParameters::ModifyBound(UStageDataAsset* Data)
{
	if (Data)
		Visit(Data, [](UStageMemberCommand* C)
		{
			if (!C->GraphParameterBindings.IsEmpty()) C->Modify();
		});
}

void FStageGraphParameters::Synchronize(UStageDataAsset* Data)
{
	if (Data)
		Visit(Data, [&](UStageMemberCommand* C)
		{
			TArray<FString> Errors;
			FStageCommandParameters::ApplyOne(C, Data->Parameters, Errors);
		});
}

bool FStageGraphParameters::IsBound(const UStageMemberCommand* Command, const FPropertyAndParent& P)
{
	if (!Command) return false;
	TArray<FString> Names;
	for (const FProperty* Parent : P.ParentProperties)
		Names.Add(Parent->GetName());
	Names.Add(P.Property.GetName());
	if (Command->GraphParameterBindings.Contains(FName(*FString::Join(Names, TEXT("."))))) return true;
	Names.Reset();
	for (int32 I = P.ParentProperties.Num() - 1; I >= 0; --I)
		Names.Add(P.ParentProperties[I]->GetName());
	Names.Add(P.Property.GetName());
	return Command->GraphParameterBindings.Contains(FName(*FString::Join(Names, TEXT("."))));
}

void FStageGraphParameters::ExtendRow(const FOnGenerateGlobalRowExtensionArgs& Args, TArray<FPropertyRowExtensionButton>& Buttons)
{
	auto Handle = Args.PropertyHandle;
	if (!Handle.IsValid() || !Handle->IsValidHandle()) return;
	TArray<UObject*> Objects;
	Handle->GetOuterObjects(Objects);
	if (Objects.Num() != 1) return;
	auto* Command = Cast<UStageMemberCommand>(Objects[0]);
	auto* Data = Command ? Command->GetTypedOuter<UStageDataAsset>() : nullptr;
	TArray<FString> Segments;
	for (auto H = Handle; H.IsValid() && H->GetProperty(); H = H->GetParentHandle())
		Segments.Insert(H->GetProperty()->GetName(), 0);
	const FName Path(*FString::Join(Segments, TEXT(".")));
	FProperty* P = nullptr;
	void* Address = nullptr;
	if (!Property(Data, Command, Path, P, Address)) return;
	FPropertyRowExtensionButton Button;
	Button.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Link"));
	Button.Label = NSLOCTEXT("StageScenario", "GraphParameterMenu", "Graph parameter…");
	const FGuid* Bound = Command->GraphParameterBindings.Find(Path);
	const auto* Desc = Bound ? Data->Parameters.FindPropertyDescByID(*Bound) : nullptr;
	Button.ToolTip = FText::FromString(Desc ? TEXT("Bound to graph parameter: ") + Desc->Name.ToString()
	        : Bound                         ? TEXT("Parameter is missing. Rebind or use local value.")
	                                        : TEXT("Promote this value or reuse a compatible graph parameter."));
	Button.UIAction = FUIAction(FExecuteAction::CreateLambda([WeakData = TWeakObjectPtr<UStageDataAsset>(Data), WeakCommand = TWeakObjectPtr<UStageMemberCommand>(Command), Path]
	{
		auto* Asset = WeakData.Get();
		auto* C = WeakCommand.Get();
		FProperty* Prop = nullptr;
		void* Value = nullptr;
		if (!Property(Asset, C, Path, Prop, Value)) return;
		auto Window = SNew(SWindow)
			.Title(NSLOCTEXT("StageScenario", "ParameterWindow", "Graph parameter"))
			.ClientSize(FVector2D(420, 360))
			.SupportsMaximize(false)
			.SupportsMinimize(false);
		TWeakPtr<SWindow> WeakWindow = Window;
		auto Name = SNew(SEditableTextBox);
		FString Suggested = Prop->GetMetaData(TEXT("StageGraphParameter"));
		if (Suggested.IsEmpty() || Suggested == TEXT("true")) Suggested = Prop->GetName();
		FString Candidate = Suggested;
		for (int32 I = 2; Asset->Parameters.FindPropertyDescByName(FName(*Candidate)); ++I)
			Candidate = Suggested + FString::FromInt(I);
		Name->SetText(FText::FromString(Candidate));
		FMenuBuilder Menu(true, nullptr);
		if (const UPropertyBag* Bag = Asset->Parameters.GetPropertyBagStruct())
			for (const auto& D : Bag->GetPropertyDescs())
				if (D.CachedProperty && Prop->SameType(D.CachedProperty))
					Menu.AddMenuEntry(FText::FromName(D.Name), NSLOCTEXT("StageScenario", "ReuseParameter", "Reuse this graph parameter"), FSlateIcon(),
					    FUIAction(FExecuteAction::CreateLambda([WeakData, WeakCommand, Path, Id = D.ID, WeakWindow]
					{
						if (Bind(WeakData.Get(), WeakCommand.Get(), Path, Id))
							if (auto W = WeakWindow.Pin()) W->RequestDestroyWindow();
					})));
		if (C->GraphParameterBindings.Contains(Path))
			Menu.AddMenuEntry(NSLOCTEXT("StageScenario", "LocalValue", "Use local value (unlink)"), FText(), FSlateIcon(),
			    FUIAction(FExecuteAction::CreateLambda([WeakData, WeakCommand, Path, WeakWindow]
			{
				if (Unbind(WeakData.Get(), WeakCommand.Get(), Path))
					if (auto W = WeakWindow.Pin()) W->RequestDestroyWindow();
			})));
		Window->SetContent
		(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(12)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("New parameter for ") + Path.ToString()))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(12, 0)
			[
				Name
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(12)
			[
				SNew(SButton)
				.Text(NSLOCTEXT("StageScenario", "Promote", "Promote to graph parameter"))
				.OnClicked_Lambda([WeakData, WeakCommand, Path, Name, WeakWindow]
				{
					if (Promote(WeakData.Get(), WeakCommand.Get(), Path, FName(*Name->GetText().ToString().TrimStartAndEnd())))
					if (auto W = WeakWindow.Pin()) W->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(12)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("StageScenario", "CompatibleParameters", "Or bind a compatible existing parameter:"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					Menu.MakeWidget()
				]
			]
		);
		FSlateApplication::Get().AddWindow(Window);
	}));
	Buttons.Add(Button);
}
