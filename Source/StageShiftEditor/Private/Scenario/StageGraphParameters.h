#pragma once
#include "CoreMinimal.h"
#include "PropertyEditorDelegates.h"
class UStageDataAsset;class UStageMemberCommand;
struct FStageGraphParameters
{
    static void ExtendRow(const FOnGenerateGlobalRowExtensionArgs& Args,TArray<FPropertyRowExtensionButton>& Buttons);
    static bool Promote(UStageDataAsset* Data,UStageMemberCommand* Command,FName Path,FName Name);
    static bool Bind(UStageDataAsset* Data,UStageMemberCommand* Command,FName Path,FGuid Id);
    static bool Unbind(UStageDataAsset* Data,UStageMemberCommand* Command,FName Path);
    static void ModifyBound(UStageDataAsset* Data);
    static void Synchronize(UStageDataAsset* Data);
    static bool IsBound(const UStageMemberCommand* Command,const FPropertyAndParent& Property);
};
