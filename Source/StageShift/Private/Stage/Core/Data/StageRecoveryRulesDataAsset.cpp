// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Core/Data/StageRecoveryRulesDataAsset.h"
#include "Stage/Core/StageAction.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UStageRecoveryRulesDataAsset::ValidateDefinition(TArray<FString>& Errors, const FString& Path) const
{
    const int32 Initial = Errors.Num();
    if (!FMath::IsFinite(RecoveryWaitTimeout) || RecoveryWaitTimeout < 0) Errors.Add(Path + TEXT(" / RecoveryWaitTimeout must be finite and nonnegative"));
    int32 Index = 0;
    for (const auto& Rule : Rules)
    {
        const FString RulePath = Path + FString::Printf(TEXT(" / Rule[%d]"), Index++);
        if (!IsValid(Rule.Action) || Rule.Action->GetClass()->HasAnyClassFlags(CLASS_Abstract) || Rule.Action->GetOuter() != this) Errors.Add(RulePath + TEXT(" / null, abstract or foreign-Outer action"));
        else Rule.Action->ValidateRecoveryTemplate(RulePath, Errors);
    }
    return Initial == Errors.Num();
}
#if WITH_EDITOR
EDataValidationResult UStageRecoveryRulesDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    TArray<FString> Errors;
    ValidateDefinition(Errors);
    for (const auto& Error : Errors) Context.AddError(FText::FromString(Error));
    return Errors.IsEmpty() ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
