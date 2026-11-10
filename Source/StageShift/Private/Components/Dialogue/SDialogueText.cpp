#include "SDialogueText.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Text/ILayoutBlock.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Framework/Text/ShapedTextCache.h"
#include "Framework/Text/SlateTextLayout.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Containers/UnrealString.h"

namespace
{
	constexpr float DotSpace = 20.0f;
	constexpr float SpringTime = 0.225f;
	constexpr float CloseTime = 0.37f;
	constexpr float BreakTime = 0.415f;

	float Ease(float Progress)
	{
		const float T = FMath::Clamp(Progress, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	FString NormalizeLineEndings(const FText& Text)
	{
		FString Result = Text.ToString();
		Result.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Result.ReplaceInline(TEXT("\r"), TEXT("\n"));
		return Result;
	}

	FPaintGeometry GlyphPaintGeometry(const FGeometry& Geometry)
	{
		// The glyphs are shaped at unit scale. The render transform applies the widget's DPI scale.
		return FPaintGeometry(FSlateLayoutTransform(Geometry.GetAccumulatedLayoutTransform().GetTranslation()),
			Geometry.GetAccumulatedRenderTransform(), Geometry.GetLocalSize(), true);
	}
}

void SDialogueText::Construct(const FArguments& Args)
{
	OnOpacityChanged = Args._OnOpacityChanged;
	SetVisibility(EVisibility::HitTestInvisible);
	SetCanTick(false);
	DotBrush = FSlateRoundedBoxBrush(FLinearColor::White);
}

void SDialogueText::SetAppearance(const FSlateFontInfo& InFont, FLinearColor InTextColor, float InWrapWidth, ETextJustify::Type InJustification)
{
	Font = InFont;
	TextColor = InTextColor;
	WrapWidth = FMath::Max(InWrapWidth, 100.0f);
	Justification = InJustification;
	Reset();
}

void SDialogueText::Layout(FLine& Line, const FText& Text)
{
	const FString Source = NormalizeLineEndings(Text);
	const TSharedRef<FSlateTextLayout> TextLayout = FSlateTextLayout::Create(this, FTextBlockStyle().SetFont(Font));
	
	TextLayout->SetJustification(Justification);
	const float RevealSpace = bShowRevealDots ? DotSpace : 0.0f;
	TextLayout->SetWrappingWidth(WrapWidth - RevealSpace);
	
	FPlainTextLayoutMarshaller::Create()->SetText(Source, *TextLayout);
	
	TextLayout->UpdateIfNeeded();
	
	Line.VisualJustification = TextLayout->GetVisualJustification();
	
	TArray<FTextRange> SourceLineRanges;
	FTextRange::CalculateLineRangesFromString(Source, SourceLineRanges);

	for (const FTextLayout::FLineView& View : TextLayout->GetLineViews())
	{
		const FString& ModelText = *TextLayout->GetLineModels()[View.ModelIndex].Text;
		const int32 ModelStart = SourceLineRanges[View.ModelIndex].BeginIndex;
		for (const TSharedRef<ILayoutBlock>& Block : View.Blocks)
		{
			const FLayoutBlockTextContext Context = Block->GetTextContext();
			
			const FShapedGlyphSequenceRef Shaped = ShapedTextCacheUtil::GetShapedTextSubSequence(
				Context.ShapedTextCache, FCachedShapedTextKey(View.Range, 1.0f, Context, Font),
				Block->GetTextRange(), *ModelText, Context.TextDirection);
			
			const TArray<FShapedGlyphEntry>& Entries = Shaped->GetGlyphsToRender();
			// Slate blocks already include their line's offset and justification.
			FVector2f Position(Block->GetLocationOffset());
			for (int32 Index = 0; Index < Entries.Num();)
			{
				const FShapedGlyphEntry& First = Entries[Index];
				const int32 Start = First.SourceIndex;
				int32 Count = 0;
				FVector2f Advance = FVector2f::ZeroVector;
				bool bVisible = false;
				TArray<FShapedGlyphEntry> Cluster;
				do
				{
					const FShapedGlyphEntry& Entry = Entries[Index++];
					Count = FMath::Max(Count, static_cast<int32>(Entry.NumCharactersInGlyph));
					Advance += FVector2f(Entry.XAdvance, Entry.YAdvance);
					bVisible |= Entry.bIsVisible;
					Cluster.Add(Entry);
				}
				while (Entries.IsValidIndex(Index) && Entries[Index].SourceIndex == Start);

				if (bVisible)
				{
					Count = FMath::Max(1, Count);
					FGlyph& Glyph = Line.Glyphs.AddDefaulted_GetRef();
					Glyph.Position = Position;
					Glyph.Width = FMath::Max(1.0f, Advance.X);
					Glyph.TextEnd = ModelStart + Start + Count;
					Glyph.bRightToLeft = First.TextDirection == TextBiDi::ETextDirection::RightToLeft;
					Glyph.Shape = MakeShared<FShapedGlyphSequence>(MoveTemp(Cluster), Shaped->GetTextBaseline(),
						Shaped->GetMaxTextHeight(), Shaped->GetFontMaterial(), Shaped->IsMaterialStencil(),
						Shaped->GetFontOutlineSettings(), FShapedGlyphSequence::FSourceTextRange(Start, Count));
				}
				Position += Advance;
			}
		}
	}
	Line.Glyphs.StableSort([](const FGlyph& A, const FGlyph& B) { return A.TextEnd < B.TextEnd; });
	Line.Size = FVector2f(TextLayout->GetSize()) + FVector2f(RevealSpace, 0.0f);
}

void SDialogueText::UpdateOpacity() const
{
	const float Incoming = Active.Id.IsValid() ? Ease(Active.Age / 0.145f) : 0.0f;
	float Outgoing = 0.0f;
	if (Exiting.Id.IsValid())
	{
		const float Fade = ExitReason == EDialogueEndReason::Completed
			? Ease(ExitAge / CloseTime)
			: Ease((ExitAge - 0.022f) / 0.23f);
		Outgoing = Ease(Exiting.Age / 0.145f) * (1.0f - Fade);
	}
	OnOpacityChanged.ExecuteIfBound(FMath::Max(Incoming, Outgoing));
}

void SDialogueText::Preview(const FText& Text)
{
	FDialoguePlaybackState State;
	State.PlaybackId = FGuid::NewGuid();
	State.Line.Text = Text;
	State.VisibleText = Text;
	State.Phase = EDialoguePlaybackPhase::Holding;
	Update(State);
	Active.Age = 1.0f;
	Active.HoldAge = 1.0f;
	SetCanTick(false);
}

void SDialogueText::Update(const FDialoguePlaybackState& State)
{
	if (Active.Id != State.PlaybackId)
	{
		Active = {};
		Active.Id = State.PlaybackId;
		Layout(Active, State.Line.Text);
		if (ExitReason == EDialogueEndReason::Completed)
		{
			Exiting = {};
		}
		Invalidate(EInvalidateWidgetReason::Layout);
	}
	const int32 VisibleEnd = NormalizeLineEndings(State.VisibleText).Len();
	for (FGlyph& Glyph : Active.Glyphs)
	{
		if (Glyph.TextEnd > VisibleEnd)
		{
			break;
		}
		if (Glyph.RevealedAt < 0.0f)
		{
			Glyph.RevealedAt = Active.Age;
			Active.LastReveal = Active.Age;
		}
	}
	Active.bHolding = State.Phase == EDialoguePlaybackPhase::Holding;
	SetCanTick(true);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SDialogueText::End(EDialogueEndReason Reason)
{
	Exiting = MoveTemp(Active);
	Active = {};
	ExitReason = Reason;
	ExitAge = 0.0f;
	SetCanTick(true);
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SDialogueText::SetShowRevealDots(bool bShowDots)
{
    bShowRevealDots = bShowDots;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SDialogueText::Reset()
{
	Active = {};
	Exiting = {};
	UpdateOpacity();
	SetCanTick(false);
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SDialogueText::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	Active.Age += DeltaTime;
	if (Active.bHolding)
	{
		Active.HoldAge += DeltaTime;
	}
	if (Exiting.Id.IsValid())
	{
		Exiting.Age += DeltaTime;
		ExitAge += DeltaTime;
		if (ExitAge >= (ExitReason == EDialogueEndReason::Completed ? CloseTime : BreakTime))
		{
			Exiting = {};
			Invalidate(EInvalidateWidgetReason::Layout);
		}
	}
	const bool bAnimating = Active.Id.IsValid() && (!Active.bHolding || Active.Age - Active.LastReveal < SpringTime || Active.HoldAge < 0.32f);
	SetCanTick(bAnimating || Exiting.Id.IsValid());
	UpdateOpacity();
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SDialogueText::ComputeDesiredSize(float LayoutScale) const
{
	return FVector2D(FMath::Max3(1.0f, Active.Size.X, Exiting.Size.X), FMath::Max3(1.0f, Active.Size.Y, Exiting.Size.Y));
}

int32 SDialogueText::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FLinearColor Tint = Style.GetColorAndOpacityTint();
	if (Exiting.Id.IsValid())
	{
		PaintLine(Exiting, Geometry, Elements, Layer + 1, Tint, true);
	}
	if (Active.Id.IsValid())
	{
		PaintLine(Active, Geometry, Elements, Layer, Tint, false);
	}
	return Layer + 1;
}

void SDialogueText::PaintLine(const FLine& Line, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 TextLayer, FLinearColor Tint, bool bExiting) const
{
	const bool bFracture = bExiting && ExitReason == EDialogueEndReason::Interrupted;
	const float Closing = bExiting && !bFracture ? Ease(ExitAge / CloseTime) : 0.0f;
	const float Arrival = Ease(Line.Age / 0.145f);
	
	float AlignmentFactor = 0.0f;

	switch (Line.VisualJustification)
	{
	case ETextJustify::Center:
		AlignmentFactor = 0.5f;
		break;

	case ETextJustify::Right:
	case ETextJustify::InvariantRight:
		AlignmentFactor = 1.0f;
		break;

	default:
		AlignmentFactor = 0.0f;
		break;
	}

	const FVector2f Origin((Geometry.GetLocalSize().X - Line.Size.X) * AlignmentFactor, Geometry.GetLocalSize().Y - Line.Size.Y);
	
	const FGeometry TextGeometry = Geometry.MakeChild(Line.Size, FSlateLayoutTransform(Origin));
	Tint.A *= Arrival * (1.0f - Closing);

	const FGlyph* Last = nullptr;
	for (int32 Index = 0; Index < Line.Glyphs.Num(); ++Index)
	{
		const FGlyph& Glyph = Line.Glyphs[Index];
		if (Glyph.RevealedAt < 0.0f)
		{
			break;
		}
		Last = &Glyph;
		const float Age = FMath::Max(0.0f, Line.Age - Glyph.RevealedAt);
		const float T = Age / SpringTime;
		const float Spring = T >= 1.0f ? 1.0f : 1.0f - FMath::Exp(-4.8f * T) * (FMath::Cos(11.0f * T) + 0.16f * FMath::Sin(11.0f * T));
		FLinearColor Ink = TextColor * Tint;
		Ink.A *= Ease(Age / 0.068f);
		const float Delay = 0.014f + (Index % 5) * 0.012f;
		const float Fracture = bFracture ? FMath::Clamp((ExitAge - Delay) / 0.32f, 0.0f, 1.0f) : 0.0f;
		PaintGlyph(Glyph, TextGeometry, Elements, TextLayer, Glyph.Position + FVector2f(0.0f, (1.0f - Spring) * 8.0f),
			Ink, 0.56f + 0.44f * Spring, Fracture, Index);
	}
	if (!bShowRevealDots || !Last || bExiting || Line.HoldAge >= 0.32f)
	{
		return;
	}
	const float Direction = Last->bRightToLeft ? -1.0f : 1.0f;
	const FVector2f DotStart = Last->Position + FVector2f(Last->bRightToLeft ? -6.0f : Last->Width + 3.0f, Last->Shape->GetMaxTextHeight() * 0.71f);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const float Beat = 0.5f + 0.5f * FMath::Sin(Line.Age * 14.3f - Index * 1.5f);
		FLinearColor DotTint = TextColor * Tint;
		DotTint.A *= (0.36f + 0.61f * Beat) * (1.0f - Ease(Line.HoldAge / 0.32f));
		const FVector2f Offset = DotStart + FVector2f(Index * 5.0f * Direction, -4.0f * Beat * Beat);
		FSlateDrawElement::MakeBox(Elements, TextLayer, TextGeometry.ToPaintGeometry(FVector2f(3.1f), FSlateLayoutTransform(Offset)),
			&DotBrush, ESlateDrawEffect::None, DotTint);
	}
}

void SDialogueText::PaintGlyph(const FGlyph& Glyph, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, FVector2f Position, FLinearColor Tint, float Scale, float Fracture, int32 Index) const
{
	const FVector2f Size(Glyph.Width, Glyph.Shape->GetMaxTextHeight());
	const FShapedGlyphSequenceRef Shape = Glyph.Shape.ToSharedRef();
	FLinearColor Outline = Font.OutlineSettings.OutlineColor;
	Outline.A *= Tint.A;
	if (Fracture <= 0.0f)
	{
		const FGeometry GlyphGeometry = Geometry.MakeChild(Size, FSlateLayoutTransform(Position),
			FSlateRenderTransform(FScale2D(1.0f, Scale)), FVector2f(0.5f, 0.8f));
		FSlateDrawElement::MakeShapedText(Elements, Layer, GlyphPaintGeometry(GlyphGeometry),
			Shape, ESlateDrawEffect::None, Tint, Outline);
		return;
	}
	if (Fracture >= 1.0f)
	{
		return;
	}

	const float Release = 1.0f - FMath::Square(1.0f - Fracture);
	const float Fall = FMath::Pow(Fracture, 1.75f);
	Tint = FMath::Lerp(Tint, FLinearColor(0.42f, 0.22f, 0.09f, Tint.A), Ease(Fracture * 3.0f));
	Tint.A *= 1.0f - Ease((Fracture - 0.29f) / 0.71f);
	Outline.A *= Tint.A;
	for (const float Side : {-1.0f, 1.0f})
	{
		const float Drift = (Index % 7 - 3) * 1.8f;
		const FVector2f Offset((Drift + Side * 11.0f) * Release, (34.0f + Side * 4.0f) * Fall + Side * 4.0f * Release);
		const float Rotation = FMath::DegreesToRadians((Side * 23.0f + (Index % 3 - 1) * 6.0f) * Release);
		const FGeometry Shard = Geometry.MakeChild(Size, FSlateLayoutTransform(Position + Offset),
			FSlateRenderTransform(FQuat2D(Rotation)), FVector2f(0.5f));
		const float Top = -3.0f;
		const float Bottom = Size.Y + 4.0f;
		const FVector2f TopCut(Size.X * 0.73f, Top);
		const FVector2f BottomCut(Size.X * 0.29f, Bottom);
		const bool bLeft = Side < 0.0f;
		const FVector2f OuterTop(bLeft ? -4.0f : Size.X + 4.0f, Top);
		const FVector2f OuterBottom(OuterTop.X, Bottom);
		Elements.PushClip(FSlateClippingZone(Shard.LocalToAbsolute(bLeft ? OuterTop : TopCut), Shard.LocalToAbsolute(bLeft ? TopCut : OuterTop),
			Shard.LocalToAbsolute(bLeft ? OuterBottom : BottomCut), Shard.LocalToAbsolute(bLeft ? BottomCut : OuterBottom)));
		FSlateDrawElement::MakeShapedText(Elements, Layer, GlyphPaintGeometry(Shard), Shape, ESlateDrawEffect::None, Tint, Outline);
		Elements.PopClip();
	}
}
