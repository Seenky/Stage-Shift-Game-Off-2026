#include "UI/Events/ClickMenuOverlayEvent.h"

#include "Engine/World.h"
#include "UI/Button3D.h"
#include "UI/MenuPlayerController.h"

void UClickMenuOverlayEvent::OnClicked_Implementation()
{
	if (const UWorld* World = GetWorld())
	{
		if (auto* Controller = Cast<AMenuPlayerController>(World->GetFirstPlayerController()))
		{
			Controller->OpenOverlay(Panel, GetTypedOuter<AButton3D>());
		}
	}
}
