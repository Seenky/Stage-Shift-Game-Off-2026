
#include "UI/Button3D.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

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

void AButton3D::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    ButtonMesh->SetMaterial(0, InactiveButtonMaterial);

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
    
    ClickEvent = DuplicateObject(ClickEvent, this);

    if (!InactiveButtonMaterial || !HoveredButtonMaterial || !ActiveButtonMaterial)
    {
        SetActorTickEnabled(false);
        return;
    }

    CurrentButtonMaterialDYN = UMaterialInstanceDynamic::Create(InactiveButtonMaterial, this);

    InactiveButtonMaterialDYN = UMaterialInstanceDynamic::Create(InactiveButtonMaterial, this);

    HoveredButtonMaterialDYN = UMaterialInstanceDynamic::Create(HoveredButtonMaterial, this);

    ActiveButtonMaterialDYN = UMaterialInstanceDynamic::Create(ActiveButtonMaterial, this);

    ButtonMesh->SetMaterial(0, CurrentButtonMaterialDYN);

    CurrentBlendState = 0.0f;

    SetActorTickEnabled(false);
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
    
    const float TargetState = bIsActive ? 2.0f : bIsHovered ? 1.0f : 0.0f;

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
    if (GetWorld()->GetTimerManager().IsTimerActive(DeactivateTimerHandle)) return;
    
    bIsActive = true;
    
    ClickEvent->OnClicked();

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
