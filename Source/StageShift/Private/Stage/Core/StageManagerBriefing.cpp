#include "Stage/Core/StageManager.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
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
	Widget->SetSceneBriefing(this, PendingSceneBriefing);
	if (!Widget->AddToPlayerScreen(30)) return;

	BriefingWidget = Widget;
	bSavedBriefingCursor = Player->bShowMouseCursor;
	Player->bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(Player, Widget->GetContinueButton(), EMouseLockMode::DoNotLock, true);
}

void AStageManager::CloseSceneBriefing()
{
	PendingSceneBriefing = FText::GetEmpty();
	if (auto* Widget = BriefingWidget.Get())
	{
		BriefingWidget = nullptr;
		if (APlayerController* Player = Widget->GetOwningPlayer(); IsValid(Player) && !Player->IsActorBeingDestroyed())
		{
			UWidgetBlueprintLibrary::SetInputMode_GameOnly(Player, true);
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
