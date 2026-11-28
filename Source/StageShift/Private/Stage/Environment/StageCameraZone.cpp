#include "Stage/Environment/StageCameraZone.h"

#include "Camera/CameraActor.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AStageCameraZone::AStageCameraZone()
{
	GetCollisionComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCollisionComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AStageCameraZone::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	const APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Player || !Player->IsLocalController() || !ZoneCamera || !OutsideCamera
	    || Player->GetViewTarget() != OutsideCamera) return;

	ActiveViewer = Player;
	Player->SetViewTargetWithBlend(ZoneCamera, BlendTime, VTBlend_Cubic);
}

void AStageCameraZone::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	const APawn* Pawn = Cast<APawn>(OtherActor);
	APlayerController* Player = ActiveViewer.Get();
	if (!Pawn || !Player || Pawn->GetController() != Player) return;

	ActiveViewer.Reset();
	if (OutsideCamera && (Player->GetViewTarget() == ZoneCamera || Player->GetViewTarget() == OutsideCamera))
		Player->SetViewTargetWithBlend(OutsideCamera, BlendTime, VTBlend_Cubic);
}
