#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Scenario/StageGraphParameters.h"

class FStageShiftEditorModule : public IModuleInterface
{
    FDelegateHandle ParameterRows;
public:
    virtual void StartupModule() override
    {
        FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("StructUtilsEditor"));
        ParameterRows=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).GetGlobalRowExtensionDelegate().AddStatic(&FStageGraphParameters::ExtendRow);
    }
    virtual void ShutdownModule() override
    {if(auto* Properties=FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))Properties->GetGlobalRowExtensionDelegate().Remove(ParameterRows);}
};
IMPLEMENT_MODULE(FStageShiftEditorModule, StageShiftEditor)
