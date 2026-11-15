#pragma once
#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"

class UStageMemberCommand;

struct STAGESHIFT_API FStageCommandParameters
{
	static bool Resolve(UObject* Object, FName Path, FProperty*& Property, void*& Address);
	static bool Supports(const FProperty* Property);
	static bool ApplyOne(UStageMemberCommand* Command, const FInstancedPropertyBag& Values, TArray<FString>& Errors, const FInstancedPropertyBag* Overrides = nullptr);
	static bool ApplyTree(UStageMemberCommand* Command, const FInstancedPropertyBag& Values, TArray<FString>& Errors, const FInstancedPropertyBag* Overrides = nullptr);
};
