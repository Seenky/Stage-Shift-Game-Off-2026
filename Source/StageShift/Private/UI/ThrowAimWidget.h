#pragma once

#include "Blueprint/UserWidget.h"
#include "ThrowAimWidget.generated.h"

UCLASS()

class UThrowAimWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetBlocked() { bBlocked = true; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Clip, FSlateWindowElementList& DrawElements, int32 Layer,
	    const FWidgetStyle& Style, bool bEnabled) const override;

private:
	bool bBlocked = false;
};
