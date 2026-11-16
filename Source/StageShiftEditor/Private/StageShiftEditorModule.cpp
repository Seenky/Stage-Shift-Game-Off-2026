#include "Modules/ModuleManager.h"
#include "Core/MainCameraActor.h"
#include "Details/MainCameraActorDetails.h"
#include "PropertyEditorModule.h"
#include "Scenario/StageGraphParameters.h"
#include "Stage/Core/StageDebugDisplay.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

namespace
{
	const FName StageDebugTabId(TEXT("StageShift.StageDebug"));
}

class FStageShiftEditorModule : public IModuleInterface
{
	FDelegateHandle ParameterRows;
	bool bOpeningStageDebugTab = false;

	TSharedRef<SDockTab> SpawnStageDebugTab(const FSpawnTabArgs&)
	{
		TGuardValue<bool> OpeningTab(bOpeningStageDebugTab, true);
		if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Stage.Debug"))) Variable->Set(1, ECVF_SetByConsole);
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.OnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab>)
			{
				if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Stage.Debug"))) Variable->Set(0, ECVF_SetByConsole);
			}))
			[StageDebugDisplay::CreateEditorContent()];
	}

	void HandleStageDebugChanged(IConsoleVariable* Variable)
	{
		if (Variable->GetInt() == 0)
			StageDebugDisplay::ClearEditorContent();
		else if (!bOpeningStageDebugTab && FSlateApplication::IsInitialized())
			FGlobalTabmanager::Get()->TryInvokeTab(StageDebugTabId, true);
	}

public:
	virtual void StartupModule() override
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(StageDebugTabId, FOnSpawnTab::CreateRaw(this, &FStageShiftEditorModule::SpawnStageDebugTab))
			.SetDisplayName(FText::FromString(TEXT("Stage.Debug")))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory());
		if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Stage.Debug")))
			Variable->SetOnChangedCallback(FConsoleVariableDelegate::CreateRaw(this, &FStageShiftEditorModule::HandleStageDebugChanged));

		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("StructUtilsEditor"));
		auto& Properties = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		ParameterRows = Properties.GetGlobalRowExtensionDelegate().AddStatic(&FStageGraphParameters::ExtendRow);
		Properties.RegisterCustomClassLayout(AMainCameraActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMainCameraActorDetails::MakeInstance));
		Properties.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		if (auto* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Stage.Debug"))) Variable->SetOnChangedCallback(FConsoleVariableDelegate());
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(StageDebugTabId);
		StageDebugDisplay::ClearEditorContent();

		if (auto* Properties = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			Properties->UnregisterCustomClassLayout(AMainCameraActor::StaticClass()->GetFName());
			Properties->GetGlobalRowExtensionDelegate().Remove(ParameterRows);
			Properties->NotifyCustomizationModuleChanged();
		}
	}
};
IMPLEMENT_MODULE(FStageShiftEditorModule, StageShiftEditor)
