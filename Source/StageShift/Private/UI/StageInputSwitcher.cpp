#include "UI/StageInputSwitcher.h"

#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "UI/StageInputDeviceSubsystem.h"

void UStageInputSwitcher::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (IsDesignTime()) return;

	const UUserWidget* Owner = GetTypedOuter<UUserWidget>();
	const ULocalPlayer* Player = Owner ? Owner->GetOwningLocalPlayer() : nullptr;
	auto* Subsystem = Player ? Player->GetSubsystem<UStageInputDeviceSubsystem>() : nullptr;
	if (Subsystem && InputSubsystem != Subsystem)
	{
		if (auto* Previous = InputSubsystem.Get()) Previous->OnInputDeviceChanged.RemoveAll(this);
		Subsystem->OnInputDeviceChanged.AddUObject(this, &ThisClass::ShowDevice);
		InputSubsystem = Subsystem;
	}
	if (Subsystem) ShowDevice(Subsystem->GetInputDevice());
}

void UStageInputSwitcher::ReleaseSlateResources(bool bReleaseChildren)
{
	if (auto* Subsystem = InputSubsystem.Get()) Subsystem->OnInputDeviceChanged.RemoveAll(this);
	InputSubsystem.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

void UStageInputSwitcher::ShowDevice(EStageInputDevice Device)
{
	SetActiveWidgetIndex(FMath::Min(static_cast<int32>(Device), FMath::Max(0, GetChildrenCount() - 1)));
}
