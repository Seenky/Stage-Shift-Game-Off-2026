#include "UI/StageInputDeviceSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "Input/Events.h"
#include "Slate/SGameLayerManager.h"
#include "Widgets/SViewport.h"

class FStageInputDeviceProcessor final : public IInputProcessor
{
public:
	explicit FStageInputDeviceProcessor(UStageInputDeviceSubsystem& InOwner) : Owner(InOwner) {}

	void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}

	bool HandleKeyDownEvent(FSlateApplication& Slate, const FKeyEvent& Event) override
	{
		return Observe(Slate, Event, Event.GetKey().IsGamepadKey());
	}

	bool HandleAnalogInputEvent(FSlateApplication& Slate, const FAnalogInputEvent& Event) override
	{
		const FKey& Key = Event.GetKey();
		if (!Key.IsGamepadKey() || !IsForPlayer(Slate, Event)) return false;
		const float Magnitude = FMath::Abs(Event.GetAnalogValue());
		if (Magnitude < 0.32f)
			ActiveAxes.Remove(Key);
		else if (Magnitude >= 0.48f && !ActiveAxes.Contains(Key))
		{
			ActiveAxes.Add(Key);
			Owner.SetInputDevice(GetGamepadType(Event.GetInputDeviceId()));
		}
		return false;
	}

	bool HandleMouseMoveEvent(FSlateApplication& Slate, const FPointerEvent& Event) override
	{
		if (!Event.IsTouchEvent() && Event.GetCursorDelta().SizeSquared() >= 4.0)
			return Observe(Slate, Event, false);
		return false;
	}

	bool HandleMouseButtonDownEvent(FSlateApplication& Slate, const FPointerEvent& Event) override
	{
		return Observe(Slate, Event, false);
	}

	bool HandleMouseWheelOrGestureEvent(FSlateApplication& Slate, const FPointerEvent& Event, const FPointerEvent*) override
	{
		return Observe(Slate, Event, false);
	}

private:
	bool Observe(FSlateApplication& Slate, const FInputEvent& Event, bool bGamepad)
	{
		if (IsForPlayer(Slate, Event))
			Owner.SetInputDevice(bGamepad ? GetGamepadType(Event.GetInputDeviceId()) : EStageInputDevice::KeyboardMouse);
		return false;
	}

	bool IsForPlayer(FSlateApplication& Slate, const FInputEvent& Event) const
	{
		const ULocalPlayer* Player = Owner.GetLocalPlayer();
		const auto User = Slate.GetUser(Event);
		if (!Player || !Player->ViewportClient || !User) return false;
		const bool bMatchesUser = User->GetPlatformUserId() == Player->GetPlatformUserId()
		    || static_cast<int32>(Event.GetUserIndex()) == Player->GetControllerId();
		if (!bMatchesUser) return false;

		const auto& ViewportClient = *Player->ViewportClient;
		if (User->IsWidgetInFocusPath(ViewportClient.GetGameViewportWidget())) return true;
		const auto Layers = ViewportClient.GetGameLayerManager();
		return Layers && User->IsWidgetInFocusPath(Layers->AsWidget());
	}

	static EStageInputDevice GetGamepadType(FInputDeviceId DeviceId)
	{
		const auto* Devices = UInputDeviceSubsystem::Get();
		const FHardwareDeviceIdentifier Hardware = Devices ? Devices->GetInputDeviceHardwareIdentifier(DeviceId) : FHardwareDeviceIdentifier{};
		const FString Name = Hardware.InputClassName.ToString() + TEXT(" ") + Hardware.HardwareDeviceIdentifier.ToString();
		return Name.Contains(TEXT("DualSense")) || Name.Contains(TEXT("DualShock")) || Name.Contains(TEXT("PlayStation"))
		    || Name.Contains(TEXT("PS4")) || Name.Contains(TEXT("PS5"))
		    ? EStageInputDevice::PlayStation : EStageInputDevice::Xbox;
	}

	UStageInputDeviceSubsystem& Owner;
	TSet<FKey> ActiveAxes;
};

void UStageInputDeviceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (FSlateApplication::IsInitialized())
	{
		Processor = MakeShared<FStageInputDeviceProcessor>(*this);
		FSlateApplication::Get().RegisterInputPreProcessor(Processor, EInputPreProcessorType::PreGame);
	}
}

void UStageInputDeviceSubsystem::Deinitialize()
{
	if (Processor && FSlateApplication::IsInitialized())
		FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
	Processor.Reset();
	Super::Deinitialize();
}

void UStageInputDeviceSubsystem::SetInputDevice(EStageInputDevice Device)
{
	if (InputDevice == Device) return;
	InputDevice = Device;
	OnInputDeviceChanged.Broadcast(Device);
}
