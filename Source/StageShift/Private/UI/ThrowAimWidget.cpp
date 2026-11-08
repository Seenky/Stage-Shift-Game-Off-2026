#include "UI/ThrowAimWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Rendering/DrawElements.h"

TSharedRef<SWidget> UThrowAimWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this);
	if (!WidgetTree->RootWidget) WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	return Super::RebuildWidget();
}

int32 UThrowAimWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Clip,
	FSlateWindowElementList& DrawElements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	Layer = Super::NativePaint(Args, Geometry, Clip, DrawElements, Layer, Style, bEnabled);
	const FVector2f Center(Geometry.GetLocalSize() * 0.5f);
	TArray<FVector2f> Ring;
	for (int32 I = 0; I <= 24; ++I)
	{
		const float Angle = I * UE_TWO_PI / 24.0f;
		Ring.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * 8.0f);
	}
	FSlateDrawElement::MakeLines(DrawElements, Layer + 1, Geometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, FLinearColor(0.05f, 0.025f, 0.01f, 0.85f), true, 4.0f);
	FSlateDrawElement::MakeLines(DrawElements, Layer + 2, Geometry.ToPaintGeometry(), Ring, ESlateDrawEffect::None, FLinearColor(1.0f, 0.88f, 0.58f), true, 1.5f);
	for (const FVector2f Axis : {FVector2f(1, 0), FVector2f(0, 1)})
	{
		FSlateDrawElement::MakeLines(DrawElements, Layer + 2, Geometry.ToPaintGeometry(), TArray<FVector2f>{Center - Axis * 3.0f, Center + Axis * 3.0f}, ESlateDrawEffect::None, FLinearColor(1.0f, 0.95f, 0.8f), true, 1.5f);
	}
	return Layer + 2;
}
