#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageAudience.generated.h"

class AStageManager;
class AStageSoundControl;
class UAudioComponent;
class UBoxComponent;
class UPoseableMeshComponent;
class USplineComponent;
class USkeletalMesh;
class USoundAttenuation;
class USoundBase;
enum class EStageSoundResult : uint8;
struct FStageSoundWindow;

USTRUCT(BlueprintType)
struct STAGESHIFT_API FStageAudienceSeat
{
	GENERATED_BODY()

	// The origin sits on the cushion. Rotation faces the stage.
	UPROPERTY(EditAnywhere, Category = "Audience")
	FTransform SeatTransform;

	UPROPERTY(EditAnywhere, Category = "Audience", meta = (ClampMin = "0"))
	int32 Appearance = 0;

	UPROPERTY(EditAnywhere, Category = "Audience")
	TMap<FName, FLinearColor> MaterialTints;

	UPROPERTY(EditAnywhere, Category = "Audience")
	TArray<FName> HiddenMaterialSlots;
};

UCLASS(Blueprintable)
class STAGESHIFT_API AStageAudience : public AActor
{
	GENERATED_BODY()

public:
	AStageAudience();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Stage|Audience")
	void ReactWithLaughter();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostRegisterAllComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stage|Audience")
	TObjectPtr<UBoxComponent> NoticeArea;

	UPROPERTY(EditAnywhere, Category = "Stage|Audience", meta = (ClampMin = "0", Units = "cm"))
	float NoticeStageDepth = 330.f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = "Stage|Audience")
	TObjectPtr<USplineComponent> NoticeOutline;
#endif

	UPROPERTY(EditDefaultsOnly, Category = "Stage|Audience")
	TArray<TObjectPtr<USkeletalMesh>> Appearances;

	UPROPERTY(EditInstanceOnly, Category = "Stage|Audience", meta = (TitleProperty = "Appearance"))
	TArray<FStageAudienceSeat> Seats;

	UPROPERTY(EditAnywhere, Category = "Stage|Audience", meta = (MakeEditWidget = true))
	FVector StageFocus = FVector(780, 0, 205);

	UPROPERTY(EditAnywhere, Category = "Stage|Audience")
	bool bLaughAtWrongSound = true;

	UPROPERTY(EditAnywhere, Category = "Stage|Audience")
	TObjectPtr<USoundBase> LaughSound;

	UPROPERTY(EditAnywhere, Category = "Stage|Audience")
	TObjectPtr<USoundAttenuation> LaughAttenuation;

	UPROPERTY(EditAnywhere, Category = "Stage|Audience")
	TObjectPtr<USoundBase> ApplauseSound;

	UPROPERTY(EditAnywhere, Category = "Stage|Audience", meta = (ToolTip = "Short reaction when the audience notices the player."))
	TObjectPtr<USoundBase> NoticeMurmurSound;

	UPROPERTY(EditAnywhere, Category = "Stage|Audience", meta = (ToolTip = "Short reaction when curtain opening is overdue. Uses the notice reaction when unset."))
	TObjectPtr<USoundBase> CurtainOverdueMurmurSound;

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> NoticeMurmurAudio;
	float MurmurDialogueGain = 1.f;
	double NextMurmurAt = 0.0;
	bool bWasOpeningOverdue = false;
	bool bDialogueRevealing = false;

	struct FViewer
	{
		TWeakObjectPtr<UPoseableMeshComponent> Mesh;
		TArray<FTransform> SittingPose;
		TArray<FTransform> PointingPose;
		FVector LookPoint = FVector::ZeroVector;
		float HeadYaw = 0.f;
		float HeadPitch = 0.f;
		float PointAlpha = 0.f;
		double LaughAt = -10.0;
		bool bPointWithLeftArm = false;
	};

	void RebuildViewers();
	void UpdateFocus();
	void UpdateViewer(FViewer& Viewer, int32 Index, float DeltaSeconds, double Now, const APawn* PointingTarget);
	bool IsScenePlaying() const;
	bool IsInNoticeArea(const FVector& WorldLocation) const;
	void HandleScoreChanged(int32 Amount);
#if WITH_EDITOR
	void UpdateNoticeOutline();
#endif

	UFUNCTION()
	void HandleSoundResult(const FStageSoundWindow& Window, EStageSoundResult Result);

	TArray<FViewer> Viewers;
	TWeakObjectPtr<AStageManager> StageManager;
	TWeakObjectPtr<AStageSoundControl> SoundControl;
	TWeakObjectPtr<AActor> Speaker;
	double PlayerNoticeSeconds = 0.0;
	double NextFocusCheck = 0.0;
	double NextLaughAt = 0.0;
	double ApplauseAt = -10.0;
	double PlayerNoticedAt = -1.0;
	double FocusChangedAt = 0.0;
};
