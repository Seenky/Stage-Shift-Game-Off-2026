#include "Stage/Core/Data/StageCommandParameters.h"
#include "Stage/Core/StageMemberCommand.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "UObject/UnrealType.h"
bool FStageCommandParameters::Resolve(UObject* Object,FName Path,FProperty*& Property,void*& Address)
{
    Property=nullptr;Address=nullptr;if(!Object)return false;TArray<FString> Segments;Path.ToString().ParseIntoArray(Segments,TEXT("."),false);UStruct* Type=Object->GetClass();void* Container=Object;
    for(int32 I=0;I<Segments.Num();++I)
    {
        Property=FindFProperty<FProperty>(Type,*Segments[I]);if(!Property)return false;Address=Property->ContainerPtrToValuePtr<void>(Container);
        if(I+1<Segments.Num()){auto* Struct=CastField<FStructProperty>(Property);if(!Struct)return false;Type=Struct->Struct;Container=Address;}
    }
    return Property&&Address;
}
bool FStageCommandParameters::Supports(const FProperty* P)
{
    if(!P||P->HasAnyPropertyFlags(CPF_Transient)||P->GetFName()==TEXT("GraphParameterBindings")||P->GetFName()==TEXT("DeclaredResources"))return false;
#if WITH_METADATA
    if(P->HasMetaData(TEXT("NoGraphParameter")))return false;
#endif
    if(const auto* Object=CastField<FObjectPropertyBase>(P))return !Object->PropertyClass->IsChildOf(AActor::StaticClass())&&!Object->PropertyClass->IsChildOf(UActorComponent::StaticClass())&&!Object->PropertyClass->IsChildOf(UStageMemberCommand::StaticClass());
    if(const auto* S=CastField<FStructProperty>(P)){for(TFieldIterator<FProperty> It(S->Struct);It;++It)if(!Supports(*It))return false;return true;}
    return P->IsA<FTextProperty>()||P->IsA<FStrProperty>()||P->IsA<FNameProperty>()||P->IsA<FBoolProperty>()||P->IsA<FNumericProperty>()||P->IsA<FEnumProperty>();
}
bool FStageCommandParameters::ApplyOne(UStageMemberCommand* Command,const FInstancedPropertyBag& Values,TArray<FString>& Errors,const FInstancedPropertyBag* Overrides)
{
    const int32 Before=Errors.Num();
    for(const auto& Pair:Command->GraphParameterBindings)
    {
        FProperty* Target=nullptr;void* Address=nullptr;const auto* Desc=Values.GetPropertyBagStruct()?Values.GetPropertyBagStruct()->FindPropertyDescByID(Pair.Value):nullptr;
        const FString Path=Command->GetName()+TEXT(" / ")+Pair.Key.ToString();
        if(!Desc||!Desc->CachedProperty||!Resolve(Command,Pair.Key,Target,Address)||!Supports(Target)) {Errors.Add(Path+TEXT(" / graph parameter or property is missing"));continue;}
        const FInstancedPropertyBag* Source=&Values;const FProperty* Property=Desc->CachedProperty;
        if(Overrides)if(const auto* Override=Overrides->FindPropertyDescByName(Desc->Name)){Source=Overrides;Property=Override->CachedProperty;}
        if(!Property||!Target->SameType(Property)){Errors.Add(Path+TEXT(" / graph parameter type mismatch: ")+Desc->Name.ToString());continue;}
        Target->CopyCompleteValue(Address,Property->ContainerPtrToValuePtr<void>(Source->GetValue().GetMemory()));
    }
    return Errors.Num()==Before;
}
bool FStageCommandParameters::ApplyTree(UStageMemberCommand* Command,const FInstancedPropertyBag& Values,TArray<FString>& Errors,const FInstancedPropertyBag* Overrides)
{
    const int32 Before=Errors.Num();TSet<UStageMemberCommand*> Seen;TFunction<void(UStageMemberCommand*)> Visit=[&](UStageMemberCommand* C)
    {if(!C||Seen.Contains(C))return;Seen.Add(C);ApplyOne(C,Values,Errors,Overrides);for(UStageMemberCommand* Child:C->GetChildCommands())Visit(Child);};Visit(Command);return Before==Errors.Num();
}
