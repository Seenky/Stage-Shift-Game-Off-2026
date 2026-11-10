#include "Components/Interaction/InteractionOutlineComponent.h"

#include "Components/Interaction/InteractionComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

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
	if (HasBegunPlay() && Interaction.IsValid()) OnCandidateChanged(Interaction->GetInteractionCandidate());
}

void UInteractionOutlineComponent::Deactivate()
{
	Super::Deactivate();
	ClearVisual();
}

void UInteractionOutlineComponent::OnCandidateChanged(AActor* Target)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsActive() || !Interaction.IsValid() || !Interaction->IsActive() ||
		Interaction->HasActiveInteraction() || Interaction->IsPlayingReleaseAnimation() || !Pawn || !Pawn->IsLocallyControlled())
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
		if (!VisualTarget.IsValid()) { ClearVisual(); return; }
		if (bFadingOut) return;
		bFadingOut = true;
		TransitionStartAlpha = Alpha;
		TransitionTime = 0.0f;
		if (FadeOutDuration <= 0.0f || Alpha <= 0.0f) ClearVisual();
		else SetComponentTickEnabled(true);
		return;
	}

	// One stencil owner at a time, including a new candidate arriving during fade-out.
	ClearVisual();
	if (!IsValid(Target) || !OutlineMaterial) return;
	if (!MaterialInstance)
	{
		MaterialInstance = UMaterialInstanceDynamic::Create(OutlineMaterial, this);
		PostProcess = NewObject<UPostProcessComponent>(GetOwner());
		PostProcess->bUnbound = true;
		PostProcess->bEnabled = false;
		PostProcess->Priority = 10.0f;
		PostProcess->Settings.AddBlendable(MaterialInstance, 1.0f);
		PostProcess->RegisterComponent();
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Target);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (!IsValid(Primitive) || !Primitive->IsA<UMeshComponent>() || !Primitive->IsRegistered()) continue;
		if ((Primitive->CustomDepthStencilValue & StencilBit) != 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Outline: %s already owns reserved stencil bit 128; skipped"), *Primitive->GetPathName());
			continue;
		}
		PrimitiveStates.Add({Primitive, !!Primitive->bRenderCustomDepth, Primitive->CustomDepthStencilValue, Primitive->CustomDepthStencilWriteMask});
		Primitive->SetCustomDepthStencilValue(Primitive->CustomDepthStencilValue | StencilBit);
		if (Primitive->CustomDepthStencilWriteMask != ERendererStencilMask::ERSM_Default) Primitive->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
		if (!Primitive->bRenderCustomDepth) Primitive->SetRenderCustomDepth(true);
	}
	if (PrimitiveStates.IsEmpty()) return;
	VisualTarget = Target;
	Target->OnEndPlay.AddUniqueDynamic(this, &ThisClass::OnVisualTargetEndPlay);
	bFadingOut = false;
	TransitionTime = SelectionTime = TransitionStartAlpha = 0.0f;
	Alpha = FadeInDuration <= 0.0f ? 1.0f : 0.0f;
	UpdateMaterial(OutlineThickness);
	PostProcess->bEnabled = true;
	SetComponentTickEnabled(Alpha < 1.0f || (bEnableAppearPulse && AppearPulseDuration > 0.0f));
}

void UInteractionOutlineComponent::RestorePrimitives()
{
	if (AActor* Target = VisualTarget.Get()) Target->OnEndPlay.RemoveDynamic(this, &ThisClass::OnVisualTargetEndPlay);
	for (const FPrimitiveState& State : PrimitiveStates)
	{
		if (UPrimitiveComponent* Primitive = State.Component.Get())
		{
			const bool bStillOwned = Primitive->CustomDepthStencilValue == (State.Stencil | StencilBit) &&
				Primitive->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_Default && Primitive->bRenderCustomDepth;
			if (bStillOwned)
			{
				Primitive->SetCustomDepthStencilValue(State.Stencil);
				if (Primitive->CustomDepthStencilWriteMask != State.WriteMask) Primitive->SetCustomDepthStencilWriteMask(State.WriteMask);
				if (!State.bCustomDepth) Primitive->SetRenderCustomDepth(false);
			}
			else if ((Primitive->CustomDepthStencilValue & StencilBit) != 0)
			{
				// Another feature changed the shared state. Release only our bit, keep its latest settings.
				Primitive->SetCustomDepthStencilValue(Primitive->CustomDepthStencilValue & ~StencilBit);
			}
		}
	}
	PrimitiveStates.Reset();
	VisualTarget.Reset();
}

void UInteractionOutlineComponent::ClearVisual()
{
	if (PostProcess) PostProcess->bEnabled = false;
	RestorePrimitives();
	Alpha = 0.0f;
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
	if (!VisualTarget.IsValid() || !Interaction.IsValid() || !Interaction->IsActive()) { ClearVisual(); return; }
	TransitionTime += DeltaTime;
	SelectionTime += DeltaTime;
	const float Duration = bFadingOut ? FadeOutDuration : FadeInDuration;
	const float T = Duration <= 0.0f ? 1.0f : FMath::Clamp(TransitionTime / Duration, 0.0f, 1.0f);
	const float SmoothT = T * T * (3.0f - 2.0f * T);
	Alpha = FMath::Lerp(TransitionStartAlpha, bFadingOut ? 0.0f : 1.0f, SmoothT);
	if (bFadingOut && T >= 1.0f) { ClearVisual(); return; }
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
	ClearVisual();
	Super::OnUnregister();
}

void UInteractionOutlineComponent::OnInteractionDeactivated(UActorComponent* Component)
{
	ClearVisual();
}
