#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Stage/Cues/StageCueTypes.h"
#include "StagePerformanceHUDWidget.generated.h"

class AStageManager;
class UCanvasPanel;
class UImage;
class UPanelWidget;
class UTextBlock;

struct FStagePerformanceCue
{
	UTexture2D* Icon = nullptr;
	AActor* Actor = nullptr;
	int32 Number = 0;
	EStageCueWindowPhase Phase = EStageCueWindowPhase::Hidden;
};

UCLASS(Abstract)
class STAGESHIFT_API UStagePerformanceTagWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Present(const FStagePerformanceCue& Cue);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ActorNumber;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> NumberBadge;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CueIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CueState;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor WaitingColor = FLinearColor(0.47f, 0.25f, 0.11f);
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor OpenColor = FLinearColor(0.12f, 0.46f, 0.32f);
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor ClosedColor = FLinearColor(0.62f, 0.09f, 0.07f);

private:
	int32 LastNumber = INDEX_NONE;
};

UCLASS(Abstract)
class STAGESHIFT_API UStagePerformanceHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSource(AStageManager& Stage);
	void Refresh(const AStageManager& Stage, TConstArrayView<FStagePerformanceCue> Cues, TConstArrayView<FName> CastOrder);

protected:
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> ActorLayer;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> CueTags;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SceneText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ScoreText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> StarOne;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> StarTwo;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> StarThree;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> ScoreFeedback;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> ScoreDelta;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> AudienceFeedback;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> AudienceText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> AudienceIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TSubclassOf<UStagePerformanceTagWidget> CueTagClass;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TSubclassOf<UStagePerformanceTagWidget> ActorTagClass;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UTexture2D> StarFilled;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UTexture2D> StarEmpty;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UTexture2D> ApplauseIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UTexture2D> MurmurIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor PositiveColor = FLinearColor(0.63f, 0.9f, 0.61f);
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor NegativeColor = FLinearColor(1.0f, 0.47f, 0.31f);

private:
	void HandleScoreChanged(int32 Amount);
	UFUNCTION() void HandleOpeningOverdue();
	void ShowAudienceReaction(bool bPositive);
	void PositionActorTags(const FGeometry& Geometry);
	UStagePerformanceTagWidget* GetTag(UPanelWidget& Panel, TSubclassOf<UStagePerformanceTagWidget> TagClass, int32 Index);

	TWeakObjectPtr<AStageManager> Source;
	TArray<TWeakObjectPtr<AActor>, TInlineAllocator<5>> TaggedActors;
	TArray<TWeakObjectPtr<AActor>, TInlineAllocator<4>> CastMembers;
	int32 LastScene = INDEX_NONE;
	int32 LastSceneCount = INDEX_NONE;
	int32 LastScore = 0;
	int32 LastStars = INDEX_NONE;
	int64 RecentScoreDelta = 0;
	double LastScoreAt = -10.0;
	double DeltaUntil = -1.0;
	double AudienceUntil = -1.0;
};
