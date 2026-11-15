#pragma once
#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "EditorUndoClient.h"
#include "Misc/NotifyHook.h"
#include "UObject/StrongObjectPtr.h"
#include "Scenario/StageScenarioEdGraph.h"
#include "Scenario/StageScenarioEdNode.h"
class UStageDataAsset;class UStageProductionDataAsset;class IDetailsView;class IStructureDetailsView;class SGraphEditor;class SBox;
struct FStageViewScope
{
    TWeakObjectPtr<UStageMemberCommand> Command;
    FGuid Step,Group;
    FGuid Reaction;
    bool bReactionAssignments=false;
    bool bAssignments=false;
    FString Name;
};
class FStageScenarioEditorToolkit : public FAssetEditorToolkit,public FEditorUndoClient,public FNotifyHook
{
public:
    void Init(UObject* Asset,EToolkitMode::Type Mode,TSharedPtr<IToolkitHost> Host);
    virtual ~FStageScenarioEditorToolkit();
    virtual FName GetToolkitFName() const override{return TEXT("StageScenarioEditor");}
    virtual FText GetBaseToolkitName() const override;
    virtual FString GetWorldCentricTabPrefix() const override{return TEXT("Stage");}
    virtual FLinearColor GetWorldCentricTabColorScale() const override{return FLinearColor(0.1f,0.55f,0.65f);}
    virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& Manager) override;
    virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& Manager) override;
    virtual void PostUndo(bool bSuccess) override{QueueRefresh();}
    virtual void PostRedo(bool bSuccess) override{QueueRefresh();}
    virtual void NotifyPreChange(FProperty* Property) override;
    virtual void NotifyPostChange(const FPropertyChangedEvent& Event,FProperty* Property) override;
    virtual void SaveAsset_Execute() override;
private:
    UObject* Asset=nullptr;UStageDataAsset* Data=nullptr;UStageProductionDataAsset* Production=nullptr;
    TStrongObjectPtr<UStageScenarioEdGraph> Story,Reactions;
    TSharedPtr<SGraphEditor> StoryView,ReactionView;
    TSharedPtr<SBox> StoryHost,ReactionHost;
    TSharedPtr<IDetailsView> CommandDetails,AssetDetails;
    TSharedPtr<IStructureDetailsView> ItemDetails;
    TWeakObjectPtr<UStageScenarioEdNode> Selected;
    UStageScenarioEdGraph* ActiveGraph=nullptr;
    TArray<FStageViewScope> StoryScopes,ReactionScopes;
    FString Message;bool bEditable=false,bRefreshQueued=false;
    FDelegateHandle PropertyHandle;
    TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
    void Refresh();void QueueRefresh();void Build(bool bReaction,bool bFrame=false);
    void Select(const TSet<UObject*>& Objects);void Enter(UEdGraphNode* Node);
    void Action(UStageScenarioEdGraph* Graph,EStageGraphAction Action,UClass* Class,UStageScenarioEdNode* Node,FVector2D Location);
    TArray<UStageScenarioEdNode*> Selection(UStageScenarioEdGraph* Graph,UStageScenarioEdNode* Fallback=nullptr) const;
    TSharedPtr<SGraphEditor> View(UStageScenarioEdGraph* Graph) const;
    void Validate();void Changed(UObject* Object,FPropertyChangedEvent& Event);
};
