#include "Stage/Core/StageManager.h"
#include "Stage/Core/StageDebugDisplay.h"

#include "Components/Stage/StageMemberComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Stage/Commands/WaitForStatusCommand.h"
#include "Stage/Commands/WaitForStageMarkCommand.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageAction.h"
#include "Stage/Core/StageMemberCommand.h"
#include "StatusesComponent.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

class SStageDebugWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStageDebugWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&)
	{
		SetVisibility(EVisibility::SelfHitTestInvisible);
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.025f, 0.03f, 0.04f))
			.Padding(14)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.4f).Padding(0, 0, 12, 0)[Metric(TEXT("Time"), TimeText)]
					+ SHorizontalBox::Slot().FillWidth(1.2f).Padding(0, 0, 12, 0)[Metric(TEXT("Lateness"), LatenessText)]
					+ SHorizontalBox::Slot().FillWidth(1.2f).Padding(0, 0, 12, 0)[Metric(TEXT("Step"), StepText)]
					+ SHorizontalBox::Slot().FillWidth(0.6f)[Metric(TEXT("Score"), ScoreText)]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10)[SNew(SSeparator)]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(MembersText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Mono", 11))
					.ColorAndOpacity(FLinearColor(0.88f, 0.91f, 0.96f))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 10)[SNew(SSeparator)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)
				[
					SNew(STextBlock).Text(FText::FromString(TEXT("Events")))
				]
				+ SVerticalBox::Slot().FillHeight(1)
				[
					SAssignNew(EventsScrollBox, SScrollBox)
					.ScrollBarAlwaysVisible(true)
					+ SScrollBox::Slot()
					[
						SAssignNew(EventsText, STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Mono", 10))
						.ColorAndOpacity(FLinearColor(0.7f, 0.76f, 0.85f))
						.AutoWrapText(true)
					]
				]
			]
		];
	}

	void Update(const FString& Time, const FString& Lateness, const FString& Step, int32 Score, const FString& Members, const TArray<FString>& Events)
	{
		TimeText->SetText(FText::FromString(Time));
		LatenessText->SetText(FText::FromString(Lateness));
		StepText->SetText(FText::FromString(Step));
		ScoreText->SetText(FText::AsNumber(Score));
		MembersText->SetText(FText::FromString(Members));
		UpdateEvents(Events);
	}

	void UpdateEvents(const TArray<FString>& Events)
	{
		if (LastEventCount == Events.Num()) return;
		const bool bFollowLatest = EventsScrollBox->GetScrollOffset() >= EventsScrollBox->GetScrollOffsetOfEnd() - 1.0f;
		EventsText->SetText(FText::FromString(Events.IsEmpty() ? TEXT("-") : FString::Join(Events, TEXT("\n"))));
		LastEventCount = Events.Num();
		if (bFollowLatest) EventsScrollBox->ScrollToEnd();
	}

	void Clear()
	{
		for (const auto& Text : {TimeText, LatenessText, StepText, ScoreText, MembersText, EventsText}) Text->SetText(FText::GetEmpty());
		LastEventCount = INDEX_NONE;
		EventsScrollBox->ScrollToStart();
	}

private:
	TSharedRef<SWidget> Metric(const TCHAR* Label, TSharedPtr<STextBlock>& Value)
	{
		return SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(FText::FromString(Label))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.ColorAndOpacity(FLinearColor(0.6f, 0.67f, 0.78f))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SAssignNew(Value, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Mono", 11))
				.ColorAndOpacity(FLinearColor::White).AutoWrapText(true)
			];
	}

	TSharedPtr<STextBlock> TimeText, LatenessText, StepText, ScoreText, MembersText, EventsText;
	TSharedPtr<SScrollBox> EventsScrollBox;
	int32 LastEventCount = INDEX_NONE;
};

namespace
{
	TAutoConsoleVariable<int32> CVarStageDebug(TEXT("Stage.Debug"), 0, TEXT("Show Stage debug window: 1 = on, 0 = off."), ECVF_Cheat);
#if WITH_EDITOR
	TWeakPtr<SStageDebugWidget> EditorDebugWidget;
	TWeakObjectPtr<AStageManager> EditorDebugSource;
#endif

	template <typename T> FString EnumName(T Value)
	{
		return StaticEnum<T>()->GetNameStringByValue(static_cast<int64>(Value));
	}

	FString ActionDescription(const UStageAction* Action)
	{
		return IsValid(Action) ? FString::Printf(TEXT("%s [%s]"), *Action->GetClass()->GetName(), *EnumName(Action->GetState())) : TEXT("-");
	}

	FString WaitReason(UStageMemberCommand* Command, const UStageMemberComponent* Member, EStageCommandType Type)
	{
		if (Command->GetState() == EStageExecutionState::Finished) return TEXT("-");
		if (Command->GetState() == EStageExecutionState::NotStarted) return TEXT("Pending");
		if (Type == EStageCommandType::Scenario && Member->HasReaction()) return TEXT("Reaction");
		if (Member->IsPhysicallyBlocked()) return TEXT("PhysicalRecovery");
		if (const auto* Wait = Cast<UWaitForStatusCommand>(Command); Wait && Member->GetMainAction(Command))
			return Wait->GetTargetRoleId().ToString() + TEXT(" / ") + Wait->GetStatusTag().ToString();
		if (const auto* Wait = Cast<UWaitForStageMarkCommand>(Command); Wait && Member->GetMainAction(Command))
			return Wait->GetTargetRoleId().ToString() + TEXT(" / StageMark");
		if (Member->GetMainAction(Command)) return TEXT("-");
		if (Member->GetRecoveryAction()) return TEXT("Recovery");
		if (Command->bRequireStageLocation && !Member->IsAtStageMark(true, false)) return TEXT("StageLocation");
		if (Command->bRequireStageYaw && !Member->IsAtStageMark(false, true)) return TEXT("StageYaw");
		const auto* Statuses = Member->GetOwner()->FindComponentByClass<UStatusesComponent>();
		if (Statuses && !Command->ExecutionRequirements.GetIsAccess(Statuses->GetStatuses())) return TEXT("ExecutionRequirements");
		return TEXT("Execution");
	}

	void AppendCommandDebug(UStageMemberCommand* Command, const UStageMemberComponent* Member, EStageCommandType Type, const FString& ParentPath, FString& Text)
	{
		if (!IsValid(Command)) return;
		const FString Path = ParentPath.IsEmpty() ? Command->GetClass()->GetName() : ParentPath + TEXT(" > ") + Command->GetClass()->GetName();
		bool bHasActiveChild = false;
		for (UStageMemberCommand* Child : Command->GetChildCommands())
		{
			if (!IsValid(Child) || (Child->GetState() != EStageExecutionState::Running && Child->GetState() != EStageExecutionState::Stopping)) continue;
			bHasActiveChild = true;
			AppendCommandDebug(Child, Member, Type, Path, Text);
		}
		if (bHasActiveChild) return;
		const FString State = Command->GetState() == EStageExecutionState::Finished ? EnumName(Command->GetResult()) : EnumName(Command->GetState());
		Text += FString::Printf(TEXT("  Command   %s / %s [%s]\n  Action    %s\n  Wait      %s\n"), *EnumName(Type), *Path, *State,
		    *ActionDescription(Member->GetMainAction(Command)), *WaitReason(Command, Member, Type));
	}
}

#if WITH_EDITOR
TSharedRef<SWidget> StageDebugDisplay::CreateEditorContent()
{
	TSharedRef<SStageDebugWidget> Widget = SNew(SStageDebugWidget);
	EditorDebugWidget = Widget;
	EditorDebugSource.Reset();
	return Widget;
}

void StageDebugDisplay::ClearEditorContent()
{
	if (auto Widget = EditorDebugWidget.Pin()) Widget->Clear();
	EditorDebugSource.Reset();
}
#endif

void AStageManager::RemoveDebugDisplay()
{
#if WITH_EDITOR
	if (EditorDebugSource.Get() == this)
	{
		// Keep the last snapshot in the editor tab after PIE, including EndPlay diagnostics.
		if (auto Widget = EditorDebugWidget.Pin()) Widget->UpdateEvents(RecentEvents);
		EditorDebugSource.Reset();
	}
#endif
	if (DebugWindow.IsValid())
	{
		DebugWindow->SetOnWindowClosed(FOnWindowClosed());
		if (FSlateApplication::IsInitialized()) DebugWindow->RequestDestroyWindow();
	}
	DebugWindow.Reset();
	DebugWidget.Reset();
}

void AStageManager::UpdateDebugDisplay()
{
	if (CVarStageDebug.GetValueOnGameThread() == 0 || !FSlateApplication::IsInitialized() || bManagerEndingPlay || !GetWorld() || !GetWorld()->IsGameWorld())
	{
		RemoveDebugDisplay();
		return;
	}
	TSharedPtr<SStageDebugWidget> DisplayWidget;
#if WITH_EDITOR
	if (GIsEditor)
	{
		DisplayWidget = EditorDebugWidget.Pin();
		if (!DisplayWidget.IsValid()) return;
		EditorDebugSource = this;
	}
	else
#endif
	{
		if (!DebugWidget.IsValid()) DebugWidget = SNew(SStageDebugWidget);
		DisplayWidget = DebugWidget;
		if (!DebugWindow.IsValid())
		{
			DebugWindow = SNew(SWindow)
				.Title(FText::FromString(TEXT("Stage.Debug")))
				.ClientSize(FVector2D(660, 720))
				.FocusWhenFirstShown(false)
				.ActivationPolicy(EWindowActivationPolicy::Never)
				[
					DebugWidget.ToSharedRef()
				];
			DebugWindow->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this, [this](const TSharedRef<SWindow>&)
			{
				DebugWindow.Reset();
				DebugWidget.Reset();
				CVarStageDebug->Set(0, ECVF_SetByConsole);
			}));
			FSlateApplication::Get().AddWindow(DebugWindow.ToSharedRef());
		}
	}

	const FStageProductionScene* ProductionScene = nullptr;
	if (ConfiguredProduction && !ConfiguredProduction->Scenes.IsEmpty())
		ProductionScene = &ConfiguredProduction->Scenes[ConfiguredProduction->Scenes.IsValidIndex(ProductionSceneIndex) ? ProductionSceneIndex : 0];
	UStageDataAsset* Data = ActiveStage ? ActiveStage.Get() : ProductionScene ? ProductionScene->Scenario.Get() : ConfiguredStage.Get();
	const FString Time = FString::Printf(TEXT("%.1f / %.1f s"), GetStageElapsedTime(), Data ? Data->MainDuration : 0.0f);
	const FString Lateness = FString::Printf(TEXT("%.1f s [%s]"), CurrentLatenessSeconds, *EnumName(CurrentLatenessLevel));
	FString Step = EnumName(State);
	if (ActiveStage && ActiveStage->Steps.IsValidIndex(CurrentStepIndex) && State == EStageState::Running)
		Step = FString::Printf(TEXT("%s (%d/%d)"), *ActiveStage->Steps[CurrentStepIndex].StepId.ToString(), CurrentStepIndex + 1, ActiveStage->Steps.Num());
	else if (ProductionState == EStageProductionState::Preparing)
		Step = TEXT("Preparing");

	TMap<FName, AActor*> Actors;
	if (ActiveStage)
	{
		for (const auto& Pair : Members) Actors.Add(Pair.Key, Pair.Value.Get());
	}
	else if (ProductionScene)
	{
		for (const auto& Pair : ResolveProductionBindings(*ProductionScene).Roles) Actors.Add(Pair.Key, Pair.Value.Get());
	}
	else if (Data)
		Actors = ResolveBindings(Data, {});
	TArray<FName> Roles = Data ? Data->GetRoleOptions() : TArray<FName>();
	if (Roles.IsEmpty()) Actors.GetKeys(Roles);

	FString MembersDescription;
	for (FName RoleId : Roles)
	{
		if (!MembersDescription.IsEmpty()) MembersDescription += TEXT("\n");
		AActor* Actor = Actors.FindRef(RoleId);
		const auto* Member = IsValid(Actor) && Actor->GetWorld() == GetWorld() ? Actor->FindComponentByClass<UStageMemberComponent>() : nullptr;
		MembersDescription += RoleId.ToString();
		if (!Member)
		{
			MembersDescription += TEXT(" [Missing]\n");
			continue;
		}
		MembersDescription += TEXT("\n");
		bool bHasCommand = false;
		for (const FStageRootExecution& Entry : RootExecutions)
		{
			if (Entry.RoleId != RoleId || !IsValid(Entry.Command)) continue;
			const bool bCurrentStep = Entry.CommandType == EStageCommandType::Scenario && Entry.SourceIndex == CurrentStepIndex;
			if (Entry.Result != EStageCommandResult::None && !bCurrentStep && Entry.CommandType != EStageCommandType::Finale) continue;
			AppendCommandDebug(Entry.Command, Member, Entry.CommandType, FString(), MembersDescription);
			bHasCommand = true;
		}
		if (!bHasCommand) MembersDescription += TEXT("  Command   -\n  Action    -\n");
		MembersDescription += TEXT("  Recovery  ") + ActionDescription(Member->GetRecoveryAction()) + TEXT("\n");
	}
	if (MembersDescription.IsEmpty()) MembersDescription = TEXT("Members  -");
	MembersDescription.TrimEndInline();

	DisplayWidget->Update(Time, Lateness, Step, Score, MembersDescription, RecentEvents);
}
