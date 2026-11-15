#include "StageCommandCatalog.h"
#include "Stage/Core/StageMemberCommand.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"

FText FStageCommandCatalog::Name(const UClass* Class)
{
    if(Class->HasMetaData(TEXT("DisplayName")))return Class->GetDisplayNameText();
    FString Label=Class->GetName();Label.RemoveFromEnd(TEXT("_C"));Label.RemoveFromStart(TEXT("Stage"));Label.RemoveFromEnd(TEXT("StageMemberCommand"));Label.RemoveFromEnd(TEXT("Command"));
    if(Label==TEXT("Dialogue"))Label=TEXT("Speak");if(Label==TEXT("LookTo"))Label=TEXT("Look");return FText::FromString(FName::NameToDisplayString(Label,false));
}
TArray<FStageCommandType> FStageCommandCatalog::Get()
{
    TArray<FStageCommandType> Result;TSet<FSoftClassPath> Seen;
    for(TObjectIterator<UClass> It;It;++It)
    {
        UClass* Class=*It;if(!Class->IsChildOf(UStageMemberCommand::StaticClass())||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||Class->GetName().StartsWith(TEXT("SKEL_"))||Class->GetName().StartsWith(TEXT("REINST_")))continue;
        FStageCommandType Entry;Entry.Path=FSoftClassPath(Class);Entry.Name=Name(Class);Entry.Category=Class->GetMetaData(TEXT("StageCategory"));if(Entry.Category.IsEmpty())Entry.Category=TEXT("Actions");Result.Add(Entry);Seen.Add(Entry.Path);
    }
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TSet<FTopLevelAssetPath> Derived;Registry.GetDerivedClassNames({UStageMemberCommand::StaticClass()->GetClassPathName()},{},Derived);
    FARFilter Filter;Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());Filter.bRecursiveClasses=true;
    TArray<FAssetData> Assets;Registry.GetAssets(Filter,Assets);
    for(const FAssetData& Asset:Assets)
    {
        FString Generated;if(!Asset.GetTagValue(TEXT("GeneratedClass"),Generated))continue;
        FSoftClassPath Path(FPackageName::ExportTextPathToObjectPath(Generated));if(Seen.Contains(Path)||!Derived.Contains(Path.GetAssetPath()))continue;
        FString Flags;if(Asset.GetTagValue(TEXT("ClassFlags"),Flags)&&(FCString::Strtoui64(*Flags,nullptr,10)&CLASS_Abstract))continue;
        Result.Add({Path,FText::FromString(FName::NameToDisplayString(Asset.AssetName.ToString(),false)),TEXT("Custom actions")});Seen.Add(Path);
    }
    Result.Sort([](const auto& A,const auto& B){return A.Name.ToString()<B.Name.ToString();});return Result;
}
