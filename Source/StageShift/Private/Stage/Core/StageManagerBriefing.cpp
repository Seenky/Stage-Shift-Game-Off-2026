#include "Stage/Core/StageManager.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "UI/StageBriefingWidget.h"

bool AStageManager::QueueSceneBriefing(const FText& Text, FText& Error, const UStageDataAsset* Scenario)
{
	if (!BriefingWidgetClass.LoadSynchronous())
	{
		Error = NSLOCTEXT("StageBriefing", "MissingWidget", "Назначь виджет вступления в Briefing Widget Class.");
		return false;
	}
	PendingSceneBriefing = Text;
	PendingBriefingScenario = Scenario;
	RefreshSceneBriefing();
	return true;
}

void AStageManager::RefreshSceneBriefing()
{
	if (PendingSceneBriefing.IsEmpty() || BriefingWidget || !GetWorld()) return;
	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || !Player->HasActorBegunPlay() || !Player->IsLocalController() || !Player->GetLocalPlayer()) return;
	TSubclassOf<UStageBriefingWidget> WidgetClass = BriefingWidgetClass.LoadSynchronous();
	if (!WidgetClass) return;
	if (bShowingProductionResult)
	{
		if (const auto ResultClass = WidgetClass.GetDefaultObject()->GetProductionResultClass()) WidgetClass = ResultClass;
	}
	auto* Widget = CreateWidget<UStageBriefingWidget>(Player, WidgetClass);
	if (!Widget) return;
	if (bShowingProductionResult) Widget->SetProductionResult(this, PendingSceneBriefing, GetScore(), GetStarCount(), StarThresholds.Num());
	else
	{
		const auto* Scenario = PendingBriefingScenario.Get();
		const TConstArrayView<FStageBriefingProp> Props = Scenario ? MakeArrayView(Scenario->BriefingProps) : TConstArrayView<FStageBriefingProp>();
		Widget->SetSceneBriefing(this, PendingSceneBriefing, Props);
	}
	if (!Widget->AddToPlayerScreen(30)) return;

	BriefingWidget = Widget;
	bSavedBriefingCursor = Player->bShowMouseCursor;
	Player->bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(Player, Widget->GetContinueButton(), EMouseLockMode::DoNotLock, true);
}

void AStageManager::CloseSceneBriefing()
{
	PendingSceneBriefing = FText::GetEmpty();
	PendingBriefingScenario.Reset();
	bShowingProductionResult = false;
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
	if (bShowingProductionResult)
	{
		if (ResultReturnLevel.IsNull())
		{
			OutError = NSLOCTEXT("StageBriefing", "MissingResultReturnLevel", "Укажи карту хаба в Result Return Level.");
			return false;
		}
		CloseSceneBriefing();
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, ResultReturnLevel);
		OutError = FText::GetEmpty();
		return true;
	}
	if (PendingSceneBriefing.IsEmpty() || bConfirmingSceneBriefing || bManagerEndingPlay)
	{
		OutError = NSLOCTEXT("StageBriefing", "NotWaiting", "Сцена уже запущена.");
		return false;
	}
	if (RuntimeCurtain.IsValid() && IsProductionActive())
	{
		CloseSceneBriefing();
		BeginCurtainOpening(ProductionSceneIndex == 0);
		OutError = FText::GetEmpty();
		return true;
	}
	TGuardValue<bool> Confirming(bConfirmingSceneBriefing, true);
	return IsProductionActive() ? ConfirmProductionPreparation(OutError) : StartStageByIds(ConfiguredStage, {}, OutError);
}

void AStageManager::ShowProductionResult()
{
	const int32 TotalStars = StarThresholds.Num();
	const int32 Stars = GetStarCount();
	const FString Rating = FString::ChrN(Stars, TEXT('★')) + FString::ChrN(TotalStars - Stars, TEXT('☆'));
	bShowingProductionResult = true;
	FText Error;
	if (!QueueSceneBriefing(FText::Format(NSLOCTEXT("StageBriefing", "ResultText", "{0}\nЗвёзды: {1} из {2}\nОчки: {3}"),
	    FText::FromString(Rating), FText::AsNumber(Stars), FText::AsNumber(TotalStars), FText::AsNumber(GetScore())), Error))
	{
		bShowingProductionResult = false;
		AddDiagnostic(Error.ToString());
	}
}
