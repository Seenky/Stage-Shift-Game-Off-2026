#pragma once

#include "Components/Widget.h"
#include "Fonts/SlateFontInfo.h"
#include "StageDialogueTypes.h"
#include "DialogueTextWidget.generated.h"

class SDialogueText;

UCLASS(meta = (DisplayName = "Dialogue Text"))
class STAGESHIFT_API UDialogueTextWidget : public UWidget
{
	GENERATED_BODY()

public:
	UDialogueTextWidget(const FObjectInitializer& ObjectInitializer);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
	void UpdatePlayback(const FDialoguePlaybackState& State);
	void EndPlayback(FGuid PlaybackId, EDialogueEndReason Reason);
	void ResetPlayback();
	void SetShowRevealDots(bool bShowDots);

	TDelegate<void(float)> OnOpacityChanged;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FSlateFontInfo Font;
	
	UPROPERTY(EditAnywhere, Category = "Appearance")
	TEnumAsByte<ETextJustify::Type> Justification = ETextJustify::Left;

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FLinearColor TextColor = FLinearColor(0.061f, 0.030f, 0.020f);

	UPROPERTY(EditAnywhere, Category = "Appearance", meta = (ClampMin = "100", UIMin = "100"))
	float TextWrapWidth = 360.0f;

	UPROPERTY(EditAnywhere, Category = "Appearance")
	FText PreviewText;

	UPROPERTY(EditAnywhere, Category = "Appearance")
	bool bShowRevealDots = true;

private:
	void ForwardOpacity(float Opacity);

	FDialoguePlaybackState CurrentState;
	TSharedPtr<SDialogueText> Text;
};
