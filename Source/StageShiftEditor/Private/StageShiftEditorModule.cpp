#include "Modules/ModuleManager.h"
#include "Core/MainCameraActor.h"
#include "ContentBrowserMenuContexts.h"
#include "Details/MainCameraActorDetails.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "Logging/MessageLog.h"
#include "Misc/ScopedSlowTask.h"
#include "PropertyEditorModule.h"
#include "Scenario/StageGraphParameters.h"
#include "Scripts/StageShiftPropIconUtility.h"
#include "Stage/Core/StageDebugDisplay.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

namespace
{
	const FName StageDebugTabId(TEXT("StageShift.StageDebug"));

	void RenderSelectedBriefingIcons(const FToolMenuContext& MenuContext)
	{
		const auto* Selection = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (!Selection) return;
		FMessageLog Log(TEXT("AssetTools"));
		TArray<UPackage*> Packages;
		FScopedSlowTask Progress(Selection->SelectedAssets.Num(), NSLOCTEXT("StageShift", "RenderingBriefingIcons", "Rendering briefing icons"));
		Progress.MakeDialog(true);
		bool bHasErrors = false;
		for (const FAssetData& Asset : Selection->SelectedAssets)
		{
			if (Progress.ShouldCancel()) break;
			Progress.EnterProgressFrame(1.0f, FText::FromName(Asset.AssetName));
			auto* Mesh = Cast<UStaticMesh>(Asset.GetAsset());
			FText Error;
			if (auto* Icon = UStageShiftPropIconUtility::RenderBriefingIcon(Mesh, FRotator::ZeroRotator, Error))
				Packages.AddUnique(Icon->GetPackage());
			else
			{
				Log.Error(FText::Format(NSLOCTEXT("StageShift", "BriefingIconError", "{0}: {1}"), FText::FromName(Asset.AssetName), Error));
				bHasErrors = true;
			}
		}
		if (!Packages.IsEmpty() && !UEditorLoadingAndSavingUtils::SavePackages(Packages, true))
		{
			Log.Error(NSLOCTEXT("StageShift", "BriefingIconSaveError", "Could not save the generated briefing icons."));
			bHasErrors = true;
		}
		if (bHasErrors) Log.Open(EMessageSeverity::Error);
	}
}

class FStageShiftEditorModule : public IModuleInterface
{
	FDelegateHandle ParameterRows;
	bool bOpeningStageDebugTab = false;

	void RegisterBriefingIconMenu()
	{
		FToolMenuOwnerScoped Owner(this);
		auto& Section = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.StaticMesh")->FindOrAddSection("GetAssetActions");
		Section.AddMenuEntry("StageShiftRenderBriefingIcon",
		    NSLOCTEXT("StageShift", "RenderBriefingIcon", "Render Briefing Icon"),
		    NSLOCTEXT("StageShift", "RenderBriefingIconTooltip", "Render a transparent prop thumbnail into StageSignals/BriefingIcons."),
		    FSlateIcon(), FToolMenuExecuteAction::CreateStatic(&RenderSelectedBriefingIcons));
	}

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
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FStageShiftEditorModule::RegisterBriefingIconMenu));
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
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
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
