#pragma once

#include "Components/PrimitiveComponent.h"

// The post-process material reads these bits and leaves all other stencil IDs available.
struct FInteractionOutlineStencil
{
	static constexpr int32 PickupCue = 1 << 5;
	static constexpr int32 Available = 1 << 6;
	static constexpr int32 Focused = 1 << 7;

	bool Apply(UPrimitiveComponent& Primitive, int32 Mask);
	bool Update(int32 Mask);
	void Reset();

private:
	TWeakObjectPtr<UPrimitiveComponent> Component;
	int32 OriginalStencil = 0;
	int32 AppliedMask = 0;
	ERendererStencilMask OriginalWriteMask = ERendererStencilMask::ERSM_Default;
	bool bOriginalCustomDepth = false;
};
