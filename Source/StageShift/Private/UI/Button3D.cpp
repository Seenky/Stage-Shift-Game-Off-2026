
#include "UI/Button3D.h"

#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult AButton3D::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Parent = Super::IsDataValid(Context);
	if (!IsValid(ClickEvent) && !bAllowUnboundClickFeedback)
	{
		Context.AddError(FText::FromString(TEXT("Button3D requires ClickEvent.")));
		return EDataValidationResult::Invalid;
	}
	if (!FeedbackMaterialSlot.IsNone() && ButtonMesh->GetStaticMesh() && GetFeedbackMaterialIndex() == INDEX_NONE)
	{
		Context.AddError(FText::FromString(TEXT("Button3D feedback material slot is missing from the mesh.")));
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif

AButton3D::AButton3D()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	SetRootComponent(ButtonMesh);

	ButtonRepresentationWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ButtonRepresentationWidget"));
	ButtonRepresentationWidget->SetupAttachment(ButtonMesh);
	ButtonRepresentationWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ButtonMesh->OnClicked.AddDynamic(this, &ThisClass::OnButtonClicked);
	ButtonMesh->OnBeginCursorOver.AddDynamic(this, &ThisClass::OnButtonStartHover);
	ButtonMesh->OnEndCursorOver.AddDynamic(this, &ThisClass::OnButtonEndHover);
}

int32 AButton3D::GetFeedbackMaterialIndex() const
{
	return FeedbackMaterialSlot.IsNone() ? 0 : ButtonMesh->GetMaterialIndex(FeedbackMaterialSlot);
}

void AButton3D::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const int32 MaterialIndex = GetFeedbackMaterialIndex();
	if (MaterialIndex != INDEX_NONE)
	{
		ButtonMesh->SetMaterial(MaterialIndex, InactiveButtonMaterial);
	}

	if (!ButtonMesh->GetStaticMesh())
	{
		ButtonRepresentationWidget->SetRelativeLocation(FVector::ZeroVector);
		return;
	}

	const FBox ButtonBounds = ButtonMesh->GetStaticMesh()->GetBoundingBox();

	const FVector WidgetOffset(ButtonBounds.Max.X + 10.0f, ButtonBounds.GetCenter().Y, ButtonBounds.GetCenter().Z);

	ButtonRepresentationWidget->SetRelativeLocation(WidgetOffset);
}

void AButton3D::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(ClickEvent)) ClickEvent = DuplicateObject(ClickEvent, this);

	const int32 MaterialIndex = GetFeedbackMaterialIndex();
	if (MaterialIndex == INDEX_NONE || !InactiveButtonMaterial || !HoveredButtonMaterial || !ActiveButtonMaterial)
	{
		SetActorTickEnabled(false);
		return;
	}

	CurrentButtonMaterialDYN = UMaterialInstanceDynamic::Create(InactiveButtonMaterial, this);

	InactiveButtonMaterialDYN = UMaterialInstanceDynamic::Create(InactiveButtonMaterial, this);

	HoveredButtonMaterialDYN = UMaterialInstanceDynamic::Create(HoveredButtonMaterial, this);

	ActiveButtonMaterialDYN = UMaterialInstanceDynamic::Create(ActiveButtonMaterial, this);

	ButtonMesh->SetMaterial(MaterialIndex, CurrentButtonMaterialDYN);

	CurrentBlendState = 0.0f;
	RestLocation = GetActorLocation();
	if (bTintLabelWithMaterial)
	{
		ButtonRepresentationWidget->InitWidget();
		if (auto* Label = ButtonRepresentationWidget->GetUserWidgetObject()) Label->SetColorAndOpacity(IdleLabelTint);
	}
	SetActorTickEnabled(bNavigationFocused || bIsHovered);
}

void AButton3D::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateMaterialBlend(DeltaTime);
}

void AButton3D::UpdateMaterialBlend(float DeltaTime)
{
	if (!CurrentButtonMaterialDYN || !InactiveButtonMaterialDYN || !HoveredButtonMaterialDYN || !ActiveButtonMaterialDYN)
	{
		SetActorTickEnabled(false);
		return;
	}

	const bool bHighlighted = bNavigationFocused || (bUsePointerFeedback && bIsHovered);
	const float TargetState = bIsActive ? 2.0f : bHighlighted ? 1.0f : 0.0f;

	const float BlendSpeed = 1.0f / FMath::Max(MaterialBlendTime, KINDA_SMALL_NUMBER);

	CurrentBlendState = FMath::FInterpConstantTo(CurrentBlendState, TargetState, DeltaTime, BlendSpeed);

	if (CurrentBlendState <= 1.0f)
	{
		CurrentButtonMaterialDYN->K2_InterpolateMaterialInstanceParams(InactiveButtonMaterialDYN, HoveredButtonMaterialDYN, CurrentBlendState);
	}
	else
	{
		CurrentButtonMaterialDYN->K2_InterpolateMaterialInstanceParams(HoveredButtonMaterialDYN, ActiveButtonMaterialDYN, CurrentBlendState - 1.0f);
	}
	const float FocusAmount = FMath::Min(CurrentBlendState, 1.f);
	if (bTintLabelWithMaterial)
	{
		if (auto* Label = ButtonRepresentationWidget->GetUserWidgetObject())
		{
			Label->SetColorAndOpacity(FMath::Lerp(IdleLabelTint, FocusedLabelTint, FocusAmount));
		}
	}
	if (FocusDepthOffset > 0.f)
	{
		SetActorLocation(RestLocation + GetActorForwardVector() * (FocusDepthOffset * FocusAmount));
	}

	if (FMath::IsNearlyEqual(CurrentBlendState, TargetState, KINDA_SMALL_NUMBER))
	{
		CurrentBlendState = TargetState;
		SetActorTickEnabled(false);
	}
}

void AButton3D::RequestMaterialUpdate()
{
	if (CurrentButtonMaterialDYN)
	{
		SetActorTickEnabled(true);
	}
}

void AButton3D::OnButtonClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
	ActivateButton();
}

void AButton3D::SetNavigationFocus(bool bFocused, bool bUsePointer, bool bPlaySound)
{
	if (bNavigationFocused == bFocused && bUsePointerFeedback == bUsePointer) return;

	bNavigationFocused = bFocused;
	bUsePointerFeedback = bUsePointer;
	if (bFocused && bPlaySound && HoverSound) UGameplayStatics::PlaySound2D(this, HoverSound);
	RequestMaterialUpdate();
}

void AButton3D::ActivateButton()
{
	if ((!IsValid(ClickEvent) && !bAllowUnboundClickFeedback) || !GetWorld()) return;
	if (GetWorld()->GetTimerManager().IsTimerActive(DeactivateTimerHandle)) return;

	bIsActive = true;

	if (PressSound) UGameplayStatics::SpawnSound2D(this, PressSound, 1.f, 1.f, 0.f, nullptr, true);
	if (IsValid(ClickEvent)) ClickEvent->OnClicked();

	RequestMaterialUpdate();

	GetWorldTimerManager().SetTimer(DeactivateTimerHandle, this, &ThisClass::DeactivateButton, ActiveHoldTime, false);
}

void AButton3D::DeactivateButton()
{
	bIsActive = false;

	RequestMaterialUpdate();
}

void AButton3D::OnButtonStartHover(UPrimitiveComponent* TouchedComponent)
{
	if (bIsHovered)
	{
		return;
	}

	bIsHovered = true;
	if (bUsePointerFeedback && HoverSound) UGameplayStatics::PlaySound2D(this, HoverSound);

	RequestMaterialUpdate();
}

void AButton3D::OnButtonEndHover(UPrimitiveComponent* TouchedComponent)
{
	if (!bIsHovered)
	{
		return;
	}

	bIsHovered = false;

	RequestMaterialUpdate();
}
