#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "StageInputDeviceSubsystem.generated.h"

class IInputProcessor;
class FStageInputDeviceProcessor;

UENUM(BlueprintType)
enum class EStageInputDevice : uint8
{
	KeyboardMouse,
	Xbox,
	PlayStation
};

DECLARE_MULTICAST_DELEGATE_OneParam(FStageInputDeviceChanged, EStageInputDevice);

UCLASS()
class STAGESHIFT_API UStageInputDeviceSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Input")
	EStageInputDevice GetInputDevice() const { return InputDevice; }

	FStageInputDeviceChanged OnInputDeviceChanged;

private:
	friend class FStageInputDeviceProcessor;
	void SetInputDevice(EStageInputDevice Device);

	EStageInputDevice InputDevice = EStageInputDevice::KeyboardMouse;
	TSharedPtr<IInputProcessor> Processor;
};
