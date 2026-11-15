#pragma once
#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

struct FStageCommandType
{
	FSoftClassPath Path;
	FText Name;
	FString Category;
};

// Loaded native classes and AssetRegistry Blueprint generated classes share one menu.
struct FStageCommandCatalog
{
	static TArray<FStageCommandType> Get();
	static FText Name(const UClass* Class);
};
