#include "UI/MenuExitWidget.h"

#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"

void UMenuExitWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::ConfirmExit);
}

void UMenuExitWidget::ConfirmExit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, true);
}
