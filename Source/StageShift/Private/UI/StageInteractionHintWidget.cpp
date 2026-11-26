#include "UI/StageInteractionHintWidget.h"

#include "Components/WidgetSwitcher.h"

void UStageInteractionHintWidget::Present(bool bSoundSelection)
{
	Hints->SetActiveWidgetIndex(bSoundSelection ? 1 : 0);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}
