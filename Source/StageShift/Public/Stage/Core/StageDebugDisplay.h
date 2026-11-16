#pragma once

#include "CoreMinimal.h"

class SWidget;

#if WITH_EDITOR
namespace StageDebugDisplay
{
	STAGESHIFT_API TSharedRef<SWidget> CreateEditorContent();
	STAGESHIFT_API void ClearEditorContent();
}
#endif
