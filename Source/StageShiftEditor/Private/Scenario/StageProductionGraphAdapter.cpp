#include "StageProductionGraphAdapter.h"
#include "StageScenarioGraphAdapter.h"
#include "StageGraphOrganization.h"
#include "StageGraphDrafts.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "Scenario/StageScenarioEdNode.h"
#include "Scenario/StageScenarioGraphSchema.h"
#include "Stage/Core/Data/StageProductionDataAsset.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "ScopedTransaction.h"
namespace
{
    FGuid Id(const UStageProductionDataAsset* Data,int32 Index,bool bPreparation)
    {
        const auto& S=Data->Scenes[Index];const FGuid Key=bPreparation?S.PreparationGuid:S.EditorGuid;if(Key.IsValid())return Key;
        FString Text=Data->GetPathName()+S.SceneId.ToString()+FString::FromInt(Index)+(bPreparation?TEXT("Prep"):TEXT("Scene"));return FGuid(FCrc::StrCrc32(*Text),FCrc::StrCrc32(*(Text+TEXT("Production"))),0,3);
    }
}
FGuid FStageProductionGraphAdapter::SceneKey(const UStageProductionDataAsset* Asset,int32 Index,bool bPreparation){return Id(Asset,Index,bPreparation);}
void FStageProductionGraphAdapter::EnsureIdentities()
{
    Data->SetFlags(RF_Transactional);Data->Modify();for(int32 I=0;I<Data->Scenes.Num();++I){Data->Scenes[I].EditorGuid=Id(Data,I,false);Data->Scenes[I].PreparationGuid=Id(Data,I,true);}
}
bool FStageProductionGraphAdapter::Add(int32 After)
{
    if(!FStageScenarioGraphAdapter::Backup(Data))return false;const FScopedTransaction Transaction(NSLOCTEXT("StageScenario","AddScene","Add production scene"));EnsureIdentities();
    FStageProductionScene S;for(int32 I=1;;++I){S.SceneId=FName(*FString::Printf(TEXT("Scene%d"),I));if(!Data->Scenes.ContainsByPredicate([&](const auto& V){return V.SceneId==S.SceneId;}))break;}
    S.EditorGuid=FGuid::NewGuid();S.PreparationGuid=FGuid::NewGuid();Data->Scenes.Insert(S,After==INDEX_NONE?Data->Scenes.Num():FMath::Clamp(After+1,0,Data->Scenes.Num()));Data->MarkPackageDirty();Data->PostEditChange();return true;
}
bool FStageProductionGraphAdapter::AddInView(UStageScenarioEdGraph* Graph,const UStageScenarioEdNode* From,FVector2D Position)
{
    return FStageGraphDrafts::AddScene(Graph,Position);
}
bool FStageProductionGraphAdapter::Preparation(int32 Index,bool bEnable)
{
    if(!Data->Scenes.IsValidIndex(Index)||!FStageScenarioGraphAdapter::Backup(Data))return false;const FScopedTransaction Transaction(NSLOCTEXT("StageScenario","PrepareScene","Change scene preparation"));EnsureIdentities();
    Data->Scenes[Index].bPrepareBefore=bEnable;Data->MarkPackageDirty();Data->PostEditChange();return true;
}
bool FStageProductionGraphAdapter::Delete(const UStageScenarioEdNode* Node)
{
    if(!Node||!Data->Scenes.IsValidIndex(Node->StepIndex))return false;if(Node->Kind==EStageScenarioNodeKind::Preparation)return Preparation(Node->StepIndex,false);
    if(Node->Kind!=EStageScenarioNodeKind::Scene||!FStageScenarioGraphAdapter::Backup(Data))return false;
    const FScopedTransaction Transaction(NSLOCTEXT("StageScenario","DeleteScene","Delete production scene"));EnsureIdentities();Data->Scenes.RemoveAt(Node->StepIndex);Data->MarkPackageDirty();Data->PostEditChange();return true;
}
bool FStageProductionGraphAdapter::MoveAfter(const UStageScenarioEdGraph* Graph,const UStageScenarioEdNode* Node,const UStageScenarioEdNode* After)
{
    if(!Node||!After||Node==After||Node->Kind==EStageScenarioNodeKind::Preparation||!FStageScenarioGraphAdapter::Backup(Data))return false;
    FScopedTransaction Transaction(NSLOCTEXT("StageScenario","SceneOrder","Reorder production scenes"));EnsureIdentities();
    int32 ScopeStart=0;if(Graph->ActiveGroup.IsValid())for(int32 I=0;I<Data->Scenes.Num();++I)if(Graph->Nodes.ContainsByPredicate([&](const UEdGraphNode* N){return CastChecked<UStageScenarioEdNode>(N)->RepresentedIds.Contains(Data->Scenes[I].EditorGuid);})){ScopeStart=I;break;}
    TArray<FStageProductionScene> Moving;Data->Scenes.RemoveAll([&](const auto& S){if(Node->RepresentedIds.Contains(S.EditorGuid)){Moving.Add(S);return true;}return false;});
    if(Moving.IsEmpty()){Transaction.Cancel();return false;}int32 Index=ScopeStart;
    if(After->Kind!=EStageScenarioNodeKind::Start)for(int32 I=0;I<Data->Scenes.Num();++I)if(After->RepresentedIds.Contains(Data->Scenes[I].EditorGuid)||After->RepresentedIds.Contains(Data->Scenes[I].PreparationGuid))Index=I+1;
    for(int32 I=0;I<Moving.Num();++I)Data->Scenes.Insert(Moving[I],Index+I);Data->MarkPackageDirty();Data->PostEditChange();return true;
}
void FStageProductionGraphAdapter::Build(UStageScenarioEdGraph* Graph) const
{
    Graph->Nodes.Reset();Graph->Production=Data;Graph->Schema=UStageScenarioGraphSchema::StaticClass();Graph->ScopeId=FGuid(1,0,0,4);Graph->bLinear=true;
    TArray<UStageScenarioEdNode*> Logical;
    for(int32 I=0;I<Data->Scenes.Num();++I)
    {
        const auto& S=Data->Scenes[I];auto Make=[&](bool bPreparation)
        {auto* N=NewObject<UStageScenarioEdNode>(Graph,NAME_None,RF_Transactional);N->Kind=bPreparation?EStageScenarioNodeKind::Preparation:EStageScenarioNodeKind::Scene;N->StepIndex=I;N->NodeGuid=Id(Data,I,bPreparation);N->bCanRenameNode=!bPreparation;N->AllocateDefaultPins();Logical.Add(N);return N;};
        if(S.bPrepareBefore){auto* N=Make(true);N->Title=FText::FromString(TEXT("Prepare · ")+S.SceneId.ToString());N->Summary=S.PreparationPrompt.ToString()+FString::Printf(TEXT("\n%d placements · %s"),S.Placements.Num(),S.bAutoStartWhenReady?TEXT("start when ready"):TEXT("wait for confirmation"));}
        auto* N=Make(false);N->Title=FText::FromString(FString::Printf(TEXT("%s\n%d · Scene"),*S.SceneId.ToString(),I+1));
        N->Summary=S.Scenario?FString::Printf(TEXT("%s · %.0f s%s\nDouble-click to edit scene"),*S.Scenario->GetName(),S.Scenario->MainDuration,S.Scenario->TimingPolicy==EStageTimingPolicy::FixedMainDuration?TEXT(" fixed"):TEXT(" maximum")):TEXT("Choose a Scenario in Details");
    }
    FStageGraphOrganization::Project(Graph,Logical);FStageGraphDrafts::Project(Graph);Graph->NotifyGraphChanged();
}
