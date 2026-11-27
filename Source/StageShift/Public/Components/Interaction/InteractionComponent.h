// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "InteractionTypes.h"
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

class UAnimInstance;
class UEnhancedInputComponent;
class UInputAction;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInteractionCandidateChanged, AActor*);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))

class STAGESHIFT_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	void BindInput(UEnhancedInputComponent& Input, const UInputAction* Action);

	UFUNCTION(BlueprintCallable, Category = "Interaction Component")
	EInteractionResult BeginInteraction(const FInteractionContext& InteractionContext, FInteractionPayload& OutPayload);

	UFUNCTION(BlueprintCallable, Category = "Interaction Component")
	bool EndInteraction();

	UFUNCTION(BlueprintCallable, Category = "Interaction Component")
	void CancelInteraction();

	UFUNCTION(BlueprintPure, Category = "Interaction Component")

	const FInteractionPayload& GetActiveInteraction() const { return ActiveInteraction; }

	UFUNCTION(BlueprintPure, Category = "Interaction Component")

	bool HasActiveInteraction() const { return ActiveInteraction.IsValid(); }

	bool IsPreparingRelease() const { return HasActiveInteraction() && ActiveResult == EInteractionResult::Active && ReleaseMontageId == INDEX_NONE; }

	float GetInteractionHoldDuration() const;

	bool IsPlayingReleaseAnimation() const;
	bool IsReleaseImminent() const;

	const FInteractionAnimation* FindAnimationForTarget(AActor* Target) const;
	static FVector GetControlGripLocation(const UStaticMeshComponent& Mesh, FName SocketName);
	void PlayHandMotion(EInteractionHandMotion Motion, const FVector& GripLocation, const USceneComponent* Anchor = nullptr, FName Socket = NAME_None);
	void ReleaseHandGrip(const USceneComponent& Anchor);
	void SetCatchPreview(bool bShow);
	bool SampleHandMotion(FInteractionHandPose& OutPose) const;

	// Shared by the preview and BeginInteraction; never dispatches OnInteraction.
	void RefreshInteractionCandidate(FGameplayTag ActionTag = FGameplayTag());

	AActor* GetInteractionCandidate() const { return CandidateHit.GetActor(); }

	FOnInteractionCandidateChanged OnInteractionCandidateChanged;
	virtual void Deactivate() override;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float CapsuleRadius = 65.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float CapsuleHalfHeight = 88.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (ClampMin = 0.0f, UIMin = 0.0f))
	float InteractionLength = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	bool bDebug = false;

	// Keys come from the target's GetInteractionAnimationId implementation.
	UPROPERTY(EditAnywhere, Category = "Interaction|Animation")
	TMap<FName, FInteractionAnimation> Animations;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(Transient)
	FInteractionPayload ActiveInteraction;

	UPROPERTY(Transient)
	FInteractionContext ActiveContext;

	EInteractionResult ActiveResult = EInteractionResult::Rejected;
	double PressStartTime = 0.0;
	bool bDispatchingInteraction = false;
	bool bEndingPlay = false;
	FHitResult CandidateHit;
	bool CanResumeInteraction() const;
	FHitResult ResolveInteractionCandidate(FGameplayTag ActionTag, bool bWhileCarrying = false) const;
	FHitResult ResolveHandoffCandidate() const;
	bool TryHandOff();
	void SetInteractionCandidate(const FHitResult& Hit);
	UFUNCTION()
	void OnCandidateEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	TWeakObjectPtr<UAnimInstance> PlaybackInstance;
	TWeakObjectPtr<UAnimMontage> PlayingMontage;
	int32 ReleaseMontageId = INDEX_NONE;
	float ReleasedHoldDuration = 0.0f;
	FTimerHandle ReleaseTimer;
	FName ReleaseSectionName;

	EInteractionHandMotion HandMotion = EInteractionHandMotion::None;
	TWeakObjectPtr<const USceneComponent> HandAnchor;
	FName HandAnchorSocket;
	FVector HandContact = FVector::ZeroVector;
	double HandMotionStartedAt = 0.0;
	double CatchPreviewStartedAt = -1.0;
	bool SampleCatchPreview(FInteractionHandPose& OutPose) const;

	void OnInputStarted();
	void OnInputReleased();
	const FInteractionAnimation* FindAnimation(const FInteractionPayload& Payload) const;
	bool PlaySection(const FInteractionAnimation& Animation, FName Section);
	void StopInteractionMontage();
	void OnReleaseMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 InstanceId);

	void CommitRelease();

	bool FinishInteraction(bool bCancelled);
	void ClearActiveInteraction();

	UFUNCTION()
	void OnTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);
};
