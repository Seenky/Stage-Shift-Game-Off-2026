#include "UI/StageFocusableControls.h"

#include "Components/ButtonSlot.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"

namespace
{
class SStageFocusButton final : public SButton
{
public:
	void SetStyleOwner(UStageFocusButton& InOwner) { StyleOwner = &InOwner; }

	TOptional<bool> OnQueryShowFocus(EFocusCause) const override { return false; }

	FReply OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override
	{
		bNavigationFocus = Event.GetCause() != EFocusCause::Mouse;
		Invalidate(EInvalidateWidgetReason::Paint);
		return SButton::OnFocusReceived(Geometry, Event);
	}

	void OnFocusLost(const FFocusEvent& Event) override
	{
		bNavigationFocus = false;
		Invalidate(EInvalidateWidgetReason::Paint);
		SButton::OnFocusLost(Event);
	}

	int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullRect,
	    FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override
	{
		const auto* Owner = StyleOwner.Get();
		if (!Owner || !bNavigationFocus || IsHovered() || IsPressed() || !ShouldBeEnabled(bParentEnabled))
			return SButton::OnPaint(Args, Geometry, CullRect, Elements, Layer, WidgetStyle, bParentEnabled);

		const FSlateBrush& Hover = Owner->GetStyle().Hovered;
		if (Hover.DrawAs != ESlateBrushDrawType::NoDrawType)
		{
			FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(), &Hover, ESlateDrawEffect::None,
			    Hover.GetTint(WidgetStyle) * WidgetStyle.GetColorAndOpacityTint() * GetBorderBackgroundColor().GetColor(WidgetStyle));
		}
		return SCompoundWidget::OnPaint(Args, Geometry, CullRect, Elements, Layer, WidgetStyle, bParentEnabled);
	}

private:
	TWeakObjectPtr<UStageFocusButton> StyleOwner;
	bool bNavigationFocus = false;
};

class SStageFocusSlider final : public SSlider
{
public:
	void SetStyleOwner(UStageFocusSlider& InOwner) { StyleOwner = &InOwner; }

	TOptional<bool> OnQueryShowFocus(EFocusCause) const override { return false; }

	FReply OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override
	{
		if (const auto* Owner = StyleOwner.Get(); Owner && Event.GetCause() != EFocusCause::Mouse)
		{
			FocusStyle = Owner->GetWidgetStyle();
			FocusStyle.NormalThumbImage = FocusStyle.HoveredThumbImage;
			FocusStyle.NormalBarImage = FocusStyle.HoveredBarImage;
			SetStyle(&FocusStyle);
		}
		return SSlider::OnFocusReceived(Geometry, Event);
	}

	void OnFocusLost(const FFocusEvent& Event) override
	{
		if (const auto* Owner = StyleOwner.Get()) SetStyle(&Owner->GetWidgetStyle());
		SSlider::OnFocusLost(Event);
	}

private:
	TWeakObjectPtr<UStageFocusSlider> StyleOwner;
	FSliderStyle FocusStyle;
};
}

TSharedRef<SWidget> UStageFocusButton::RebuildWidget()
{
	auto Button = SNew(SStageFocusButton)
	    .OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleClicked))
	    .OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandlePressed))
	    .OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleReleased))
	    .OnHovered_UObject(this, &ThisClass::SlateHandleHovered)
	    .OnUnhovered_UObject(this, &ThisClass::SlateHandleUnhovered)
	    .OnReceivedFocus_UObject(this, &ThisClass::SlateHandleOnReceivedFocus)
	    .OnLostFocus_UObject(this, &ThisClass::SlateHandleOnLostFocus)
	    .OnSlateButtonDragDetected(BIND_UOBJECT_DELEGATE(FOnDragDetected, SlateHandleDragDetected))
	    .OnSlateButtonDragEnter(BIND_UOBJECT_DELEGATE(FOnDragEnter, SlateHandleDragEnter))
	    .OnSlateButtonDragLeave(BIND_UOBJECT_DELEGATE(FOnDragLeave, SlateHandleDragLeave))
	    .OnSlateButtonDragOver(BIND_UOBJECT_DELEGATE(FOnDragOver, SlateHandleDragOver))
	    .OnSlateButtonDrop(BIND_UOBJECT_DELEGATE(FOnDrop, SlateHandleDrop))
	    .ButtonStyle(&GetStyle())
	    .ClickMethod(GetClickMethod())
	    .TouchMethod(GetTouchMethod())
	    .PressMethod(GetPressMethod())
	    .IsFocusable(GetIsFocusable())
	    .AllowDragDrop(bAllowDragDrop);
	Button->SetStyleOwner(*this);
	MyButton = Button;
	if (auto* ContentSlot = Cast<UButtonSlot>(GetContentSlot())) ContentSlot->BuildSlot(Button);
	return Button;
}

TSharedRef<SWidget> UStageFocusSlider::RebuildWidget()
{
	auto Slider = SNew(SStageFocusSlider)
	    .Style(&GetWidgetStyle())
	    .IsFocusable(IsFocusable)
	    .OnMouseCaptureBegin(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnMouseCaptureBegin))
	    .OnMouseCaptureEnd(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnMouseCaptureEnd))
	    .OnControllerCaptureBegin(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnControllerCaptureBegin))
	    .OnControllerCaptureEnd(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnControllerCaptureEnd))
	    .OnValueChanged(BIND_UOBJECT_DELEGATE(FOnFloatValueChanged, HandleOnValueChanged))
	    .PreventThrottling(bPreventThrottling);
	Slider->SetStyleOwner(*this);
	MySlider = Slider;
	return Slider;
}
