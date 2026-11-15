#pragma once
#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"
#include "StageScenarioGraphSchema.generated.h"

UCLASS()

class STAGESHIFTEDITOR_API UStageScenarioGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	virtual void SetNodePosition(UEdGraphNode* Node, const FVector2f& Position) const override;
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;

	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override { return FLinearColor(0.8f, 0.08f, 0.06f); }

	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;

	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override {}

	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override {}
};
