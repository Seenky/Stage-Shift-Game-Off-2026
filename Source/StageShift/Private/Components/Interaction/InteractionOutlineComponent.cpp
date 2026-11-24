#include "Components/Interaction/InteractionOutlineComponent.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Components/Interaction/InteractionInterface.h"
#include "Components/MeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Physics/PhysicsActor.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FName PickupCueEnabledParameter(TEXT("PickupCueEnabled"));
}

bool FInteractionOutlineStencil::Apply(UPrimitiveComponent& Primitive, int32 Mask)
{
	if (Component.Get() == &Primitive) return Update(Mask);
	if ((Primitive.CustomDepthStencilValue & Mask) != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Interaction outline: %s already uses reserved stencil mask %d"), *Primitive.GetPathName(), Mask);
		return false;
	}

	Component = &Primitive;
	OriginalStencil = Primitive.CustomDepthStencilValue;
	OriginalWriteMask = Primitive.CustomDepthStencilWriteMask;
	bOriginalCustomDepth = Primitive.bRenderCustomDepth;
	Update(Mask);
	if (OriginalWriteMask != ERendererStencilMask::ERSM_Default) Primitive.SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
	if (!bOriginalCustomDepth) Primitive.SetRenderCustomDepth(true);
	return true;
}

bool FInteractionOutlineStencil::Update(int32 Mask)
{
	UPrimitiveComponent* Primitive = Component.Get();
	if (!Primitive || (OriginalStencil & Mask) != 0) return false;
	const int32 Stencil = (Primitive->CustomDepthStencilValue & ~AppliedMask) | Mask;
	if (Stencil != Primitive->CustomDepthStencilValue) Primitive->SetCustomDepthStencilValue(Stencil);
	AppliedMask = Mask;
	return true;
}

void FInteractionOutlineStencil::Reset()
{
	if (UPrimitiveComponent* Primitive = Component.Get())
	{
		const bool bUnchanged = Primitive->CustomDepthStencilValue == (OriginalStencil | AppliedMask)
		    && Primitive->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_Default && Primitive->bRenderCustomDepth;
		Update(0);
		if (bUnchanged)
		{
			if (OriginalWriteMask != ERendererStencilMask::ERSM_Default) Primitive->SetCustomDepthStencilWriteMask(OriginalWriteMask);
			if (!bOriginalCustomDepth) Primitive->SetRenderCustomDepth(false);
		}
	}
	Component.Reset();
	AppliedMask = 0;
}

UInteractionOutlineComponent::UInteractionOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	bAutoActivate = true;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/MATERIALS/PostProcess/M_InteractionOutline"));
	OutlineMaterial = Material.Object;
}

void UInteractionOutlineComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickEnabled(false);
	Interaction = GetOwner()->FindComponentByClass<UInteractionComponent>();
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
		Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::OnControllerChanged);
	UpdatePostProcess();
	if (Interaction.IsValid())
	{
		CandidateChangedHandle = Interaction->OnInteractionCandidateChanged.AddUObject(this, &ThisClass::OnCandidateChanged);
		Interaction->OnComponentDeactivated.AddUniqueDynamic(this, &ThisClass::OnInteractionDeactivated);
		OnCandidateChanged(Interaction->GetInteractionCandidate());
	}
}

void UInteractionOutlineComponent::Activate(bool bReset)
{
	if (IsActive() && !bReset) return;
	Super::Activate(bReset);
	SetComponentTickEnabled(false);
	if (!HasBegunPlay()) return;
	UpdatePostProcess();
	if (Interaction.IsValid()) OnCandidateChanged(Interaction->GetInteractionCandidate());
}

void UInteractionOutlineComponent::Deactivate()
{
	Super::Deactivate();
	ClearVisual();
	UpdatePostProcess();
}

void UInteractionOutlineComponent::SetPickupCueHighlight(UObject& Source, bool bEnabled)
{
	if (bEnabled) PickupCueSources.Add(&Source);
	else PickupCueSources.Remove(&Source);
	for (auto It = PickupCueSources.CreateIterator(); It; ++It)
		if (!It->IsValid()) It.RemoveCurrent();
	UpdatePickupCueMaterial();
}

void UInteractionOutlineComponent::UpdatePickupCueMaterial()
{
	if (MaterialInstance)
		MaterialInstance->SetScalarParameterValue(PickupCueEnabledParameter, PickupCueSources.IsEmpty() ? 0.0f : 1.0f);
}

void UInteractionOutlineComponent::UpdatePostProcess()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const bool bEnabled = IsActive() && Interaction.IsValid() && OutlineMaterial && Pawn && Pawn->IsLocallyControlled();
	if (bEnabled && !PostProcess)
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(OutlineMaterial, this);
		PostProcess = NewObject<UPostProcessComponent>(GetOwner());
		PostProcess->bUnbound = true;
		PostProcess->Priority = 10.0f;
		PostProcess->Settings.AddBlendable(MaterialInstance, 1.0f);
		PostProcess->RegisterComponent();
		UpdateMaterial(OutlineThickness);
		UpdatePickupCueMaterial();
	}
	if (PostProcess) PostProcess->bEnabled = bEnabled;
}

void UInteractionOutlineComponent::OnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	ClearVisual();
	UpdatePostProcess();
	if (Interaction.IsValid()) OnCandidateChanged(Interaction->GetInteractionCandidate());
}

void UInteractionOutlineComponent::OnCandidateChanged(AActor* Target)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsActive() || !Interaction.IsValid() || !Interaction->IsActive() || Interaction->HasActiveInteraction() || Interaction->IsPlayingReleaseAnimation() || !Pawn
	    || !Pawn->IsLocallyControlled())
	{
		ClearVisual();
		return;
	}
	if (Target && Target == VisualTarget.Get())
	{
		if (!bFadingOut) return;
		bFadingOut = false;
		TransitionStartAlpha = Alpha;
		TransitionTime = 0.0f;
		SetComponentTickEnabled(true);
		return;
	}
	if (!Target)
	{
		if (!VisualTarget.IsValid() || FocusedProp.IsValid())
		{
			ClearVisual();
			return;
		}
		if (bFadingOut) return;
		bFadingOut = true;
		TransitionStartAlpha = Alpha;
		TransitionTime = 0.0f;
		if (FadeOutDuration <= 0.0f || Alpha <= 0.0f)
			ClearVisual();
		else
			SetComponentTickEnabled(true);
		return;
	}

	// One stencil owner at a time, including a new candidate arriving during fade-out.
	ClearVisual();
	if (!IsValid(Target) || !PostProcess) return;
	if (AStagePhysicsActor* Prop = Cast<AStagePhysicsActor>(Target); Prop && Prop->SetPickupOutlineFocused(true))
	{
		FocusedProp = Prop;
	}
	else
	{
		const auto* Interactable = Cast<IInteractionInterface>(Target);
		const UMeshComponent* InteractionMesh = Interactable ? Interactable->GetInteractionMesh() : nullptr;
		TInlineComponentArray<UMeshComponent*> Meshes;
		if (InteractionMesh) Meshes.Add(const_cast<UMeshComponent*>(InteractionMesh));
		else Target->GetComponents(Meshes);
		for (UMeshComponent* Primitive : Meshes)
		{
			if (!IsValid(Primitive) || !Primitive->IsRegistered()) continue;
			FInteractionOutlineStencil State;
			if (State.Apply(*Primitive, FInteractionOutlineStencil::Focused)) PrimitiveStates.Add(MoveTemp(State));
		}
		if (PrimitiveStates.IsEmpty()) return;
	}
	VisualTarget = Target;
	Target->OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnVisualTargetEndPlay);
	bFadingOut = false;
	TransitionTime = SelectionTime = TransitionStartAlpha = 0.0f;
	Alpha = FadeInDuration <= 0.0f ? 1.0f : 0.0f;
	UpdateMaterial(OutlineThickness);
	SetComponentTickEnabled(Alpha < 1.0f || (bEnableAppearPulse && AppearPulseDuration > 0.0f));
}

void UInteractionOutlineComponent::RestorePrimitives()
{
	if (AActor* Target = VisualTarget.Get()) Target->OnEndPlay.RemoveDynamic(this, &ThisClass::OnVisualTargetEndPlay);
	if (AStagePhysicsActor* Prop = FocusedProp.Get()) Prop->SetPickupOutlineFocused(false);
	FocusedProp.Reset();
	for (FInteractionOutlineStencil& State : PrimitiveStates) State.Reset();
	PrimitiveStates.Reset();
	VisualTarget.Reset();
}

void UInteractionOutlineComponent::ClearVisual()
{
	RestorePrimitives();
	Alpha = 0.0f;
	if (MaterialInstance) MaterialInstance->SetScalarParameterValue(TEXT("OutlineOpacity"), 0.0f);
	SetComponentTickEnabled(false);
}

void UInteractionOutlineComponent::UpdateMaterial(float Thickness)
{
	if (!MaterialInstance) return;
	MaterialInstance->SetVectorParameterValue(TEXT("OutlineColor"), OutlineColor);
	MaterialInstance->SetScalarParameterValue(TEXT("OutlineThickness"), FMath::Clamp(Thickness, 0.5f, 4.0f));
	MaterialInstance->SetScalarParameterValue(TEXT("OutlineOpacity"), FMath::Clamp(OutlineOpacity, 0.0f, 1.0f) * Alpha);
	MaterialInstance->SetScalarParameterValue(TEXT("DepthBias"), FMath::Max(DepthBias, 0.0f));
}

void UInteractionOutlineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Tick)
{
	Super::TickComponent(DeltaTime, TickType, Tick);
	if (!VisualTarget.IsValid() || !Interaction.IsValid() || !Interaction->IsActive())
	{
		ClearVisual();
		return;
	}
	TransitionTime += DeltaTime;
	SelectionTime += DeltaTime;
	const float Duration = bFadingOut ? FadeOutDuration : FadeInDuration;
	const float T = Duration <= 0.0f ? 1.0f : FMath::Clamp(TransitionTime / Duration, 0.0f, 1.0f);
	const float SmoothT = T * T * (3.0f - 2.0f * T);
	Alpha = FMath::Lerp(TransitionStartAlpha, bFadingOut ? 0.0f : 1.0f, SmoothT);
	if (bFadingOut && T >= 1.0f)
	{
		ClearVisual();
		return;
	}
	const bool bPulse = !bFadingOut && bEnableAppearPulse && AppearPulseDuration > 0.0f && SelectionTime < AppearPulseDuration;
	const float Pulse = bPulse ? FMath::Sin(PI * SelectionTime / AppearPulseDuration) * AppearPulseAmplitude : 0.0f;
	UpdateMaterial(OutlineThickness + Pulse);
	if (T >= 1.0f && !bPulse) SetComponentTickEnabled(false);
}

void UInteractionOutlineComponent::OnVisualTargetEndPlay(AActor* Actor, EEndPlayReason::Type Reason)
{
	ClearVisual();
}

void UInteractionOutlineComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::OnControllerChanged);
	if (Interaction.IsValid())
	{
		Interaction->OnInteractionCandidateChanged.Remove(CandidateChangedHandle);
		Interaction->OnComponentDeactivated.RemoveDynamic(this, &ThisClass::OnInteractionDeactivated);
	}
	ClearVisual();
	if (PostProcess) PostProcess->DestroyComponent();
	PostProcess = nullptr;
	Super::EndPlay(Reason);
}

void UInteractionOutlineComponent::OnUnregister()
{
	if (PostProcess) PostProcess->bEnabled = false;
	ClearVisual();
	Super::OnUnregister();
}

void UInteractionOutlineComponent::OnInteractionDeactivated(UActorComponent* Component)
{
	ClearVisual();
}
