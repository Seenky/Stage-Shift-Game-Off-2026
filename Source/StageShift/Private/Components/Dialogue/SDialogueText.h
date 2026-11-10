#pragma once

#include "Components/Dialogue/StageDialogueTypes.h"
#include "Fonts/FontCache.h"
#include "Widgets/SLeafWidget.h"

DECLARE_DELEGATE_OneParam(FOnDialogueTextOpacityChanged, float);

class SDialogueText : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDialogueText) {}
		SLATE_EVENT(FOnDialogueTextOpacityChanged, OnOpacityChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	void SetAppearance(const FSlateFontInfo& InFont, FLinearColor InTextColor, float InWrapWidth, ETextJustify::Type InJustification);
	void Preview(const FText& Text);
	void Update(const FDialoguePlaybackState& State);
	void End(EDialogueEndReason Reason);
	void Reset();
	void SetShowRevealDots(bool bShowDots);

	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
	virtual FVector2D ComputeDesiredSize(float LayoutScale) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	friend class FDialogueJustificationVerification;
	struct FGlyph
	{
		FShapedGlyphSequencePtr Shape;
		FVector2f Position;
		float Width = 0.0f;
		float RevealedAt = -1.0f;
		int32 TextEnd = 0;
		bool bRightToLeft = false;
	};

	struct FLine
	{
		TArray<FGlyph> Glyphs;
		FGuid Id;
		FVector2f Size = FVector2f::ZeroVector;
		float Age = 0.0f;
		float LastReveal = 0.0f;
		float HoldAge = 0.0f;
		bool bHolding = false;
		ETextJustify::Type VisualJustification = ETextJustify::Left;
	};

	void Layout(FLine& Line, const FText& Text);
	void PaintLine(const FLine& Line, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 TextLayer, FLinearColor Tint, bool bExiting) const;
	void PaintGlyph(const FGlyph& Glyph, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, FVector2f Position, FLinearColor Tint, float Scale, float Fracture, int32 Index) const;
	void UpdateOpacity() const;

	FSlateFontInfo Font;
	FLinearColor TextColor = {};
	ETextJustify::Type Justification = ETextJustify::Left;
	float WrapWidth = 360.0f;
	bool bShowRevealDots = true;
	FSlateBrush DotBrush;
	FOnDialogueTextOpacityChanged OnOpacityChanged;
	FLine Active;
	FLine Exiting;
	float ExitAge = 0.0f;
	EDialogueEndReason ExitReason = EDialogueEndReason::Completed;
};
