#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "StageCameraZone.generated.h"

class ACameraActor;
class APlayerController;

UCLASS()
class STAGESHIFT_API AStageCameraZone : public ATriggerBox
{
	GENERATED_BODY()

public:
	AStageCameraZone();

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

private:
	UPROPERTY(EditInstanceOnly, Category = "Camera")
	TObjectPtr<ACameraActor> ZoneCamera;

	UPROPERTY(EditInstanceOnly, Category = "Camera")
	TObjectPtr<ACameraActor> OutsideCamera;

	UPROPERTY(EditInstanceOnly, Category = "Camera", meta = (ClampMin = "0", Units = "s"))
	float BlendTime = .45f;

	TWeakObjectPtr<APlayerController> ActiveViewer;
};
