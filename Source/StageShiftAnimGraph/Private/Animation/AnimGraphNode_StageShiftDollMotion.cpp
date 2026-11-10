#include "Animation/AnimGraphNode_StageShiftDollMotion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimGraphNode_StageShiftDollMotion)

#define LOCTEXT_NAMESPACE "StageShiftDollMotion"

FText UAnimGraphNode_StageShiftDollMotion::GetControllerDescription() const
{
	return LOCTEXT("Title", "Doll Motion");
}

FText UAnimGraphNode_StageShiftDollMotion::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Applies doll compression, contact recoil and inertia while preserving the incoming foot targets.");
}

#undef LOCTEXT_NAMESPACE
