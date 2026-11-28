#include "UI/MenuPanelWidget.h"

#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UI/StageFocusableControls.h"
#include "UObject/ConstructorHelpers.h"

UMenuPanelWidget::UMenuPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static const ConstructorHelpers::FObjectFinder<USoundBase> Sound(TEXT("/Game/SOUNDS/UI/Hub/S_Select_Back"));
	BackSound = Sound.Object;
}

void UMenuPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (auto* Button = Cast<UStageFocusButton>(BackButton)) Button->SetClickSound(nullptr);
	BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
}

UWidget* UMenuPanelWidget::GetInitialFocus() const
{
	return BackButton;
}

FReply UMenuPanelWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (!Event.IsRepeat()) HandleBack();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UMenuPanelWidget::HandleBack()
{
	if (BackSound) UGameplayStatics::PlaySound2D(this, BackSound, .75f);
	RequestClose();
}

void UMenuPanelWidget::RequestClose()
{
	OnCloseRequested.Broadcast();
}
