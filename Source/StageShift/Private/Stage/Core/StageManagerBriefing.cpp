#include "Stage/Core/StageManager.h"

#include "Components/Button.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/StageBriefingWidget.h"

bool AStageManager::QueueSceneBriefing(const FText& Text, FText& Error)
{
	if (!BriefingWidgetClass.LoadSynchronous())
	{
		Error = NSLOCTEXT("StageBriefing", "MissingWidget", "Назначь виджет вступления в Briefing Widget Class.");
		return false;
	}
	PendingSceneBriefing = Text;
	RefreshSceneBriefing();
	return true;
}

void AStageManager::RefreshSceneBriefing()
{
	if (PendingSceneBriefing.IsEmpty() || BriefingWidget || !GetWorld()) return;
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || !Player->HasActorBegunPlay() || !Player->IsLocalController() || !Player->GetLocalPlayer()) return;
	auto* Widget = CreateWidget<UStageBriefingWidget>(Player, BriefingWidgetClass.LoadSynchronous());
	if (!Widget) return;
	BriefingWidget = Widget;
	Widget->SetSceneBriefing(this, PendingSceneBriefing);
	Widget->AddToPlayerScreen(30);
	bSavedBriefingCursor = Player->bShowMouseCursor;
	Player->bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetWidgetToFocus(Widget->GetContinueButton()->TakeWidget());
	InputMode.SetHideCursorDuringCapture(false);
	Player->SetInputMode(InputMode);
}

void AStageManager::CloseSceneBriefing()
{
	PendingSceneBriefing = FText::GetEmpty();
	if (auto* Widget = BriefingWidget.Get())
	{
		BriefingWidget = nullptr;
		if (APlayerController* Player = Widget->GetOwningPlayer(); IsValid(Player) && !Player->IsActorBeingDestroyed())
		{
			Player->SetInputMode(FInputModeGameOnly());
			Player->bShowMouseCursor = bSavedBriefingCursor;
		}
		Widget->RemoveFromParent();
	}
}

bool AStageManager::ContinueSceneBriefing(FText& OutError)
{
	if (PendingSceneBriefing.IsEmpty() || bConfirmingSceneBriefing || bManagerEndingPlay)
	{
		OutError = NSLOCTEXT("StageBriefing", "NotWaiting", "Сцена уже запущена.");
		return false;
	}
	TGuardValue<bool> Confirming(bConfirmingSceneBriefing, true);
	return IsProductionActive() ? ConfirmProductionPreparation(OutError) : StartStageByIds(ConfiguredStage, {}, OutError);
}
