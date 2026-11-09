#pragma once

#include "CoreMinimal.h"
#include "StageDialogueTypes.h"
#include "MVVMViewModelBase.h"

#include "DialogueViewModel.generated.h"

class UDialogueComponent;

UCLASS(BlueprintType)
class STAGESHIFT_API UDialogueViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	void Init(UDialogueComponent* InDialogueComponent);
	
	void Deinitialize();
	
	virtual void BeginDestroy() override;

	UPROPERTY(BlueprintAssignable, Category = "Dialogue")
	FOnDialogueLineEnded OnDialogueLineEnded;

	FText GetVisibleText() const { return VisibleText; }
	FGuid GetPlaybackId() const { return PlaybackId; }

private:
	void SetVisibleText(const FText& InText);
	void SetPlaybackId(const FGuid& InId);

	UFUNCTION()
	void HandleDialoguePlaybackStateChanged(const FDialoguePlaybackState& PlaybackState);

	UFUNCTION()
	void HandleDialogueLineEnded(FGuid EndedPlaybackId,EDialogueEndReason Reason);

	UPROPERTY(Transient)
	TWeakObjectPtr<UDialogueComponent> SourceDialogueComponent;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Dialogue", meta = (AllowPrivateAccess = "true"))
	FText VisibleText;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Dialogue", meta = (AllowPrivateAccess = "true"))
	FGuid PlaybackId;
};