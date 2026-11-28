#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StageDialogueVoiceLibrary.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

USTRUCT()
struct FStageDialogueVoiceProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Voice")
	TArray<TObjectPtr<USoundBase>> Syllables;

	UPROPERTY(EditAnywhere, Category = "Voice", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float Pitch = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Voice", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Volume = 0.24f;

	UPROPERTY(EditAnywhere, Category = "Voice", meta = (ClampMin = "1"))
	int32 WordsPerBeat = 2;
};

UCLASS()
class STAGESHIFT_API UStageDialogueVoiceLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	const FStageDialogueVoiceProfile& GetVoice(FName Role) const
	{
		const FStageDialogueVoiceProfile* Voice = RoleVoices.Find(Role);
		return Voice ? *Voice : DefaultVoice;
	}

	UPROPERTY(EditAnywhere, Category = "Voice")
	FStageDialogueVoiceProfile DefaultVoice;

	UPROPERTY(EditAnywhere, Category = "Voice")
	TMap<FName, FStageDialogueVoiceProfile> RoleVoices;

	UPROPERTY(EditAnywhere, Category = "Voice")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, Category = "Voice")
	TObjectPtr<USoundConcurrency> Concurrency;
};
