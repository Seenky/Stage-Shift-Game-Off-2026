#include "Animation/CharacterPhysicsAnimationSet.h"

UCharacterPhysicsAnimationSet::UCharacterPhysicsAnimationSet()
{
	FallbackAnimation.bAllowWithoutAnimation = true;
	FallbackAnimation.StrengthMultiplier = 0.0f;
}
