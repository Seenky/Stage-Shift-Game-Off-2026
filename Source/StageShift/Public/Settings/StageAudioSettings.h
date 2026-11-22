#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "StageAudioSettings.generated.h"

class USoundClass;
class USoundMix;
struct FActorsInitializedParams;

USTRUCT()
struct FStageAudioVolumes
{
	GENERATED_BODY()

	UPROPERTY()
	float Master = 1.f;

	UPROPERTY()
	float Music = 1.f;

	UPROPERTY()
	float Effects = 1.f;
};

UCLASS(Config = GameUserSettings)
class STAGESHIFT_API UStageAudioSettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	const FStageAudioVolumes& GetVolumes() const { return Volumes; }
	void SetVolumes(const FStageAudioVolumes& NewVolumes);

private:
	void OnWorldInitialized(const FActorsInitializedParams& Params);
	void ApplyToWorld(UWorld* World) const;

	UPROPERTY(Config)
	FStageAudioVolumes Volumes;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> Mix;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MusicClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> EffectsClass;

	FDelegateHandle WorldInitializedHandle;
};
