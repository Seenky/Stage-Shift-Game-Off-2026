#include "UI/MenuPanelWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"

void UMenuPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestClose);
}

UWidget* UMenuPanelWidget::GetInitialFocus() const
{
	return BackButton;
}

FReply UMenuPanelWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (!Event.IsRepeat()) RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UMenuPanelWidget::RequestClose()
{
	OnCloseRequested.Broadcast();
}
