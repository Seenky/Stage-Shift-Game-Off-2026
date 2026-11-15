#include "Modules/ModuleManager.h"
#include "Core/MainCameraActor.h"
#include "Details/MainCameraActorDetails.h"
#include "PropertyEditorModule.h"
#include "Scenario/StageGraphParameters.h"

class FStageShiftEditorModule : public IModuleInterface
{
	FDelegateHandle ParameterRows;

public:
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("StructUtilsEditor"));
		auto& Properties = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		ParameterRows = Properties.GetGlobalRowExtensionDelegate().AddStatic(&FStageGraphParameters::ExtendRow);
		Properties.RegisterCustomClassLayout(AMainCameraActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMainCameraActorDetails::MakeInstance));
		Properties.NotifyCustomizationModuleChanged();
	}

	virtual void ShutdownModule() override
	{
		if (auto* Properties = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			Properties->UnregisterCustomClassLayout(AMainCameraActor::StaticClass()->GetFName());
			Properties->GetGlobalRowExtensionDelegate().Remove(ParameterRows);
			Properties->NotifyCustomizationModuleChanged();
		}
	}
};
IMPLEMENT_MODULE(FStageShiftEditorModule, StageShiftEditor)
