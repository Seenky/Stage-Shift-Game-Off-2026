#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "StageCondition.generated.h"

class AStageManager;
class UStageDataAsset;

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class STAGESHIFT_API UStageCondition : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Stage|Branch")
	bool Evaluate(AStageManager* Manager) const;
	virtual bool Evaluate_Implementation(AStageManager* Manager) const;
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const;
};

UCLASS(meta = (DisplayName = "Has Held Item"))
class STAGESHIFT_API UStageHasHeldItemCondition : public UStageCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (GetOptions = "GetRoleOptions"))
	FName RoleId;
	// Empty keeps the existing "any carried item" behavior.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (GetOptions = "GetTargetOptions")) FName TargetId;
	UFUNCTION() TArray<FName> GetTargetOptions() const;

	UFUNCTION()
	TArray<FName> GetRoleOptions() const;
	virtual bool Evaluate_Implementation(AStageManager* Manager) const override;
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
};

UCLASS(meta = (DisplayName = "Role Has Status"))
class STAGESHIFT_API UStageRoleHasStatusCondition : public UStageCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition", meta = (GetOptions = "GetRoleOptions")) FName RoleId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition") FGameplayTag Status;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition") bool bExpected = true;
	UFUNCTION() TArray<FName> GetRoleOptions() const;
	virtual bool Evaluate_Implementation(AStageManager* Manager) const override;
	virtual void ValidateDefinition(const UStageDataAsset* Data, const FString& Path, TArray<FString>& Errors) const override;
};

UCLASS()
class STAGESHIFT_API UStageBranch : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, Category = "Branch")
	TObjectPtr<UStageCondition> Condition;
};
