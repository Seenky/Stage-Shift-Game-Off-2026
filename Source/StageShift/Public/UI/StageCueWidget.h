#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Stage/Cues/StageCueTypes.h"
#include "StageCueWidget.generated.h"

class UBorder;
class UTextBlock;
class UProgressBar;
class UVerticalBox;
class UCanvasPanel;
class UStageCueComponent;
class AStageCueWorldMarker;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class STAGESHIFT_API UStageCueSignalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Cue")
	void UpdateSignal(UTexture2D* Icon, float TimeFraction, bool bTimed, float SecondsRemaining);
	void SetSignalWindow(float StartFraction, float EndFraction, EStageCueWindowPhase Phase);
	void SetSignalCaption(const FText& Caption);

protected:
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SignalCaption;
};

UCLASS(Abstract, Blueprintable)
class STAGESHIFT_API UStageCueCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Present(const FStageCueView& View);
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor TaskColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor ProblemColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor UpcomingColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") FLinearColor CompletedColor = FLinearColor::White;

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Accent;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NumberText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> TimeText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> TimeBar;
};

UCLASS(Abstract, Blueprintable)
class STAGESHIFT_API UStageCueMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Present(const AStageCueWorldMarker* Marker);

protected:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Badge;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NumberText;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> KindText;
};

UCLASS(Abstract, Blueprintable)
class STAGESHIFT_API UStageCueHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSource(UStageCueComponent* Component);
	UPROPERTY(EditDefaultsOnly, Category = "Presentation") TSubclassOf<UStageCueCardWidget> CardClass;
	UPROPERTY(EditDefaultsOnly, Category = "Presentation") TSubclassOf<UStageCueMarkerWidget> MarkerWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> Cards;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UVerticalBox> CardArea;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> MarkerCanvas;

private:
	void Refresh();
	TWeakObjectPtr<UStageCueComponent> Source;
	FDelegateHandle ChangedHandle;
	UPROPERTY(Transient) TArray<TObjectPtr<UStageCueCardWidget>> CardWidgets;
	UPROPERTY(Transient) TMap<TObjectPtr<AStageCueWorldMarker>, TObjectPtr<UStageCueMarkerWidget>> MarkerWidgets;
};
