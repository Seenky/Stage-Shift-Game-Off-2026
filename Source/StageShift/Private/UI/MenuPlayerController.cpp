#include "UI/MenuPlayerController.h"

#include "Components/Widget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "UI/Button3D.h"
#include "UI/MenuExitWidget.h"
#include "UI/MenuPanelWidget.h"
#include "UI/SettingsWidget.h"
#include "Widgets/SViewport.h"

void AMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;

	for (TActorIterator<AButton3D> It(GetWorld()); It; ++It)
	{
		if (It->GetNavigationOrder() >= 0) Buttons.Add(*It);
	}
	Buttons.StableSort([](const AButton3D& A, const AButton3D& B)
	{
		return A.GetNavigationOrder() < B.GetNavigationOrder();
	});
	if (!Buttons.IsEmpty()) SetNavigationFocus(0, false);
}

void AMenuPlayerController::OpenOverlay(EMenuOverlay Panel, AButton3D* Origin)
{
	if (!IsLocalController() || ActiveOverlay) return;

	TSubclassOf<UMenuPanelWidget> PanelClass;
	switch (Panel)
	{
	case EMenuOverlay::Settings:
		PanelClass = SettingsClass;
		break;
	case EMenuOverlay::Coop:
		PanelClass = CoopClass;
		break;
	case EMenuOverlay::Exit:
		PanelClass = ExitClass;
		break;
	}
	if (!ensureMsgf(PanelClass, TEXT("Menu panel class is missing for %s."), *UEnum::GetValueAsString(Panel))) return;

	auto* Widget = CreateWidget<UMenuPanelWidget>(this, PanelClass);
	if (!Widget || !Widget->AddToPlayerScreen()) return;
	Widget->OnCloseRequested.AddUObject(this, &ThisClass::CloseOverlay);

	ActiveOverlay = Widget;
	if (const int32 Index = Buttons.IndexOfByKey(Origin); Index != INDEX_NONE)
	{
		SetNavigationFocus(Index, false);
	}
	bSavedClickEvents = bEnableClickEvents;
	bSavedMouseOverEvents = bEnableMouseOverEvents;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	bStickDeflected = false;

	UWidget* InitialFocus = Widget->GetInitialFocus();
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(InitialFocus->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	InitialFocus->SetUserFocus(this);
}

void AMenuPlayerController::CloseOverlay()
{
	if (!ActiveOverlay) return;

	ActiveOverlay->OnCloseRequested.RemoveAll(this);
	ActiveOverlay->RemoveFromParent();
	ActiveOverlay = nullptr;
	bEnableClickEvents = bSavedClickEvents;
	bEnableMouseOverEvents = bSavedMouseOverEvents;

	FInputModeGameAndUI Mode;
	if (const auto* LocalPlayer = GetLocalPlayer(); LocalPlayer && LocalPlayer->ViewportClient)
	{
		Mode.SetWidgetToFocus(LocalPlayer->ViewportClient->GetGameViewportWidget());
	}
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
}

bool AMenuPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Buttons.IsEmpty() || ActiveOverlay) return Super::InputKey(Params);

	const FKey& Key = Params.Key;
	if ((Key.IsMouseButton() && Params.Event == IE_Pressed)
	    || ((Key == EKeys::MouseX || Key == EKeys::MouseY) && !FMath::IsNearlyZero(Params.AmountDepressed)))
	{
		SetNavigationFocus(INDEX_NONE);
	}
	if (Key == EKeys::Gamepad_LeftY)
	{
		const float Axis = Params.AmountDepressed;
		if (FMath::Abs(Axis) < .35f) bStickDeflected = false;
		else if (FMath::Abs(Axis) >= .65f && !bStickDeflected)
		{
			bStickDeflected = true;
			MoveFocus(Axis > 0.f ? -1 : 1);
		}
		return true;
	}
	if (Params.Event == IE_Pressed || Params.Event == IE_Repeat)
	{
		if (Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Up)
		{
			MoveFocus(-1);
			return true;
		}
		if (Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Down)
		{
			MoveFocus(1);
			return true;
		}
	}
	if (Params.Event == IE_Pressed && (Key == EKeys::Gamepad_FaceButton_Bottom || Key == EKeys::Enter))
	{
		if (FocusIndex == INDEX_NONE) SetNavigationFocus(0);
		if (IsValid(Buttons[FocusIndex])) Buttons[FocusIndex]->ActivateButton();
		return true;
	}
	return Super::InputKey(Params);
}

void AMenuPlayerController::SetNavigationFocus(int32 Index, bool bPlaySound)
{
	if (Index == FocusIndex) return;

	FocusIndex = Index;
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		if (IsValid(Buttons[i])) Buttons[i]->SetNavigationFocus(i == Index, Index == INDEX_NONE, bPlaySound);
	}
}

void AMenuPlayerController::MoveFocus(int32 Direction)
{
	const int32 Index = FocusIndex == INDEX_NONE
	    ? (Direction < 0 ? Buttons.Num() - 1 : 0)
	    : (FocusIndex + Direction + Buttons.Num()) % Buttons.Num();
	SetNavigationFocus(Index);
}
