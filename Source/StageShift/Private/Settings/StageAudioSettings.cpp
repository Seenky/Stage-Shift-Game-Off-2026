#include "Settings/StageAudioSettings.h"

#include "AudioDevice.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

void UStageAudioSettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Mix = LoadObject<USoundMix>(nullptr, TEXT("/Game/SOUNDS/Settings/SMX_PlayerVolume.SMX_PlayerVolume"));
	MusicClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/SOUNDS/Settings/SC_StageMusic.SC_StageMusic"));
	EffectsClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/SOUNDS/Settings/SC_StageEffects.SC_StageEffects"));
	WorldInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &ThisClass::OnWorldInitialized);
	ApplyToWorld(GetGameInstance()->GetWorld());
}

void UStageAudioSettings::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitializedHandle);
	Super::Deinitialize();
}

void UStageAudioSettings::SetVolumes(const FStageAudioVolumes& NewVolumes)
{
	Volumes = NewVolumes;
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
	ApplyToWorld(GetGameInstance()->GetWorld());
}

void UStageAudioSettings::OnWorldInitialized(const FActorsInitializedParams& Params)
{
	ApplyToWorld(Params.World);
}

void UStageAudioSettings::ApplyToWorld(UWorld* World) const
{
	if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() == NM_DedicatedServer
	    || !Mix || !MusicClass || !EffectsClass) return;

	if (FAudioDeviceHandle Audio = World->GetAudioDevice())
	{
		const float Master = FMath::Clamp(Volumes.Master, 0.f, 1.f);
		const float Music = FMath::Clamp(Volumes.Music, 0.f, 1.f);
		const float Effects = FMath::Clamp(Volumes.Effects, 0.f, 1.f);
		Audio->SetSoundMixClassOverride(Mix, MusicClass, Master * Music, 1.f, .15f, false);
		Audio->SetSoundMixClassOverride(Mix, EffectsClass, Master * Effects, 1.f, .15f, false);
		Audio->SetBaseSoundMix(Mix);
	}
}
