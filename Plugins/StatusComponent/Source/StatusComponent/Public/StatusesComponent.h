// Statuses Component by GAIDJIIN

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "StatusesInfo.h"
#include "StatusesComponent.generated.h"

// Temporary Struct

USTRUCT(BlueprintType)
struct FTemporaryStatusInfo
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
    FGameplayTag TemporaryStatus;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Statuses Component")
    FTimerHandle TemporaryStatusTimerHandle;


    FORCEINLINE bool operator==(const FTemporaryStatusInfo& OtherStatusInfo) const
    {
        return OtherStatusInfo.TemporaryStatus == TemporaryStatus;
    }
	
    FTemporaryStatusInfo() {}

    FTemporaryStatusInfo(const FGameplayTag NewTemporaryStatus, const FTimerHandle NewTemporaryStatusTimerHandle) :
    TemporaryStatus(NewTemporaryStatus), TemporaryStatusTimerHandle(NewTemporaryStatusTimerHandle) {}
    
    FTemporaryStatusInfo(const FGameplayTag NewTemporaryStatus) : TemporaryStatus(NewTemporaryStatus) {}
	
};

FORCEINLINE uint32 GetTypeHash(const FTemporaryStatusInfo& OtherInteractInputInfo)
{
    return GetTypeHash(OtherInteractInputInfo.TemporaryStatus);
}


// Declare delegates for statuses
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAddStatuses,FGameplayTagContainer, AddStatuses);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRemoveStatuses,FGameplayTagContainer,RemoveStatuses);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChangeCurrentActiveStatus,FGameplayTag,NewActiveStatus);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class STATUSCOMPONENT_API UStatusesComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UStatusesComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // Blueprint access main functions
    
    // Getter
    // Get Statuses Info
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        FGameplayTag GetCurrentActiveStatus() const { return CurrentActiveStatus; }
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        bool GetStatusesInfo(const FGameplayTagContainer& StatusesToGet, TArray<FStatusesInfo>& ReturnStatuses) const;
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        bool GetStatusState(const FGameplayTag& StatusToCheck, TEnumAsByte<EStatusState>& StatusState) const;
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        bool GetStatusesInfoByState(const TEnumAsByte<EStatusState> StatusState, TArray<FStatusesInfo>& ReturnStatusesInfo) const;
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        bool GetStatusesByState(const TEnumAsByte<EStatusState> StatusState, FGameplayTagContainer& ReturnStatuses) const;
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        const FGameplayTagContainer& GetStatuses() const {return Statuses;}
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        bool GetAllStatusesInfo(TArray<FStatusesInfo>& ReturnStatusesInfo) const;
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        bool GetIsContainStatus(const FGameplayTag& StatusToFind, const bool ExactCheck = true, const bool InverseCondition = false) const;
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        bool GetIsContainStatuses(const FGameplayTagContainer& StatusesToFind, const bool CheckAll = false, const bool ExactCheck = true, const bool InverseCondition = false) const;
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter")
        bool GetIsEqualCurrentActiveStatus(const FGameplayTag& StatusToCompare) { return StatusToCompare == CurrentActiveStatus; }
    
    // Get Debug Info
    UFUNCTION(BlueprintCallable, Category="Statuses Component|Getter|Debug Info")
        const FText GetStatusesReadableText(const FGameplayTagContainer StatusesToText) const;
    
    // Setter
    // If bIsChangeActiveStatus == true - remove previous active status and set new, else add new active status 
    UFUNCTION(Server, Unreliable, WithValidation, BlueprintCallable, Category="Statuses Component|Add Logic", meta=(AdvancedDisplay="bIsChangeActiveStatus"))
        void ChangeOrAddCurrentActiveStatus(const FGameplayTag& NewCurrentActiveStatus, bool bIsChangeActiveStatus = true);
    UFUNCTION(Server, Unreliable, WithValidation, BlueprintCallable, Category="Statuses Component|Add Logic")
        void AddConstantStatuses(const FGameplayTagContainer& ConstantStatuses);
    UFUNCTION(Server, Unreliable, WithValidation, BlueprintCallable, Category="Statuses Component|Add Logic")
        void AddStatusesWithInfo(const FStatusesInfoArray& StatusesToAdd);
    UFUNCTION(Server, Unreliable, WithValidation, BlueprintCallable, Category="Statuses Component|Remove Logic")
        void RemoveActiveStatus();
    UFUNCTION(Server, Unreliable, WithValidation, BlueprintCallable, Category="Statuses Component|Remove Logic")
        void RemoveStatuses(const FGameplayTagContainer& StatusesToRemove);
    UFUNCTION(Server, Unreliable, WithValidation, BlueprintCallable, Category="Statuses Component|Remove Logic")
        void RemoveAllStatuses();

    // Delegates
    UPROPERTY(BlueprintAssignable, Category="Statuses Component|Delegates")
        FOnAddStatuses OnAddStatuses;
    UPROPERTY(BlueprintAssignable, Category="Statuses Component|Delegates")
        FOnRemoveStatuses OnRemoveStatuses;
    UPROPERTY(BlueprintAssignable, Category="Statuses Component|Delegates", meta = (ForceAsFunction))
        FOnChangeCurrentActiveStatus OnChangeCurrentActiveStatus;
    
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;
    
private:
    // Variables

    // Statuses Info
    UPROPERTY(Replicated)
        FGameplayTagContainer Statuses;
    // Current active status - work as enum for current status
    UPROPERTY(Replicated)
        FGameplayTag CurrentActiveStatus;
    UPROPERTY(Replicated)
        TArray<FTemporaryStatusInfo> TemporaryTags;

    UPROPERTY(EditAnywhere, Category="Status")
        FGameplayTagContainer StartStatuses;

    // Functions
    
    // Debug
    UPROPERTY(EditAnywhere, Category="Statuses Component|Debug", meta=(bAllowPrivateAccess))
        bool bShowDebug = false;

    #if !UE_BUILD_SHIPPING
        void ShowDebug();
    #endif

    // Temporary Logic
    bool ClearTemporaryStatusTimer(const FGameplayTag& StatusToClear);
    bool MakeTemporaryStatus(const FGameplayTag& StatusToAdd, const float TimeToDeleteStatus, const bool bClearTimer);
    
    // Getter
    bool GetStatusInfo(const FGameplayTag& StatusToGet, FStatusesInfo& ReturnStatusInfo) const;

    // Add Logic
    bool AddStatus(const FStatusesInfo& StatusToAdd);
    bool AddStatuses(const FGameplayTagContainer& StatusesToAdd);
    bool AddTemporaryStatuses(const FGameplayTagContainer& StatusesToAdd, const float TimeToDeleteStatuses, const bool bClearTimer = false);
};