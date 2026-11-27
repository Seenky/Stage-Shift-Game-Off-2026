#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "StageShiftPropIconUtility.generated.h"

class UStaticMesh;
class UTexture2D;

UCLASS()
class STAGESHIFTEDITOR_API UStageShiftPropIconUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Render a square, transparent briefing icon from an existing prop mesh. Rerenders update the same texture asset. */
	UFUNCTION(BlueprintCallable, Category = "Stage Shift|Briefing")
	static UTexture2D* RenderBriefingIcon(UStaticMesh* Mesh, FRotator MeshRotation, FText& Error);
};
