#include "Animation/AnimGraphNode_CharacterHairDynamics.h"

#define LOCTEXT_NAMESPACE "CharacterHairDynamics"

FText UAnimGraphNode_CharacterHairDynamics::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Character Hair Dynamics");
}

FText UAnimGraphNode_CharacterHairDynamics::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Runs Unreal AnimDynamics for the strands configured in the hair style. Supports template animation blueprints and different hair skeletons.");
}

FString UAnimGraphNode_CharacterHairDynamics::GetNodeCategory() const
{
	return TEXT("Stage Shift|Hair");
}

#undef LOCTEXT_NAMESPACE
