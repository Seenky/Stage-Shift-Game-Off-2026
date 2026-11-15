#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "Stage/Core/Data/StageDataAsset.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UStageProductionDataAsset::ValidateDefinition(TArray<FString>& Errors) const
{
    const int32 Initial = Errors.Num();
    if (ProductionId.IsNone()) Errors.Add(GetName() + TEXT(" / ProductionId is required"));
    if (Scenes.IsEmpty()) Errors.Add(GetName() + TEXT(" / at least one scene is required"));
    TSet<FName> Ids;
    for (const auto& Entry : Scenes)
    {
        const FString Path = GetName() + TEXT(" / Scene[") + Entry.SceneId.ToString() + TEXT("]");
        if (Entry.SceneId.IsNone() || Ids.Contains(Entry.SceneId)) Errors.Add(Path + TEXT(" / empty or duplicate SceneId"));
        Ids.Add(Entry.SceneId);
        if (!Entry.Scenario) Errors.Add(Path + TEXT(" / scenario is required"));
        else { TArray<FString> SceneErrors; Entry.Scenario->ValidateDefinition(SceneErrors,&Parameters); for (const auto& Error : SceneErrors) Errors.Add(Path + TEXT(" / ") + Error); }
        if (!FMath::IsFinite(Entry.PreparationTimeout) || Entry.PreparationTimeout < 0) Errors.Add(Path + TEXT(" / preparation timeout must be finite and nonnegative"));
        TSet<FName> Props;
        for (const auto& Placement : Entry.Placements)
        {
            if (Placement.PropId.IsNone() || Placement.MarkId.IsNone() || Placement.PropId == Placement.MarkId || Props.Contains(Placement.PropId))
                Errors.Add(Path + TEXT(" / Placements / unique prop and distinct mark IDs are required"));
            Props.Add(Placement.PropId);
            if (!FMath::IsFinite(Placement.PositionTolerance) || Placement.PositionTolerance < 0 || !FMath::IsFinite(Placement.YawTolerance) || Placement.YawTolerance < 0 || Placement.YawTolerance > 180 ||
                !FMath::IsFinite(Placement.MaxSpeed) || Placement.MaxSpeed < 0 || !FMath::IsFinite(Placement.MaxAngularSpeed) || Placement.MaxAngularSpeed < 0)
                Errors.Add(Path + TEXT(" / Placements / invalid tolerances or speed limits"));
        }
    }
    return Errors.Num() == Initial;
}
#if WITH_EDITOR
EDataValidationResult UStageProductionDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    const auto Parent = Super::IsDataValid(Context); TArray<FString> Errors; ValidateDefinition(Errors);
    for (const auto& Error : Errors) Context.AddError(FText::FromString(Error));
    return Errors.IsEmpty() && Parent != EDataValidationResult::Invalid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
