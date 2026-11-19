// Fill out your copyright notice in the Description page of Project Settings.


#include "Stage/Environment/StageDoor.h"

#include "Components/SceneComponent.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif


AStageDoor::AStageDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	SetRootComponent(RootComp);
}

void AStageDoor::BeginPlay()
{
	if (const USceneComponent* Root = GetRootComponent(); Root && !AppliedPreviewOffset.IsIdentity())
	{
		SetActorRelativeRotation(Root->GetRelativeRotation().Quaternion() * AppliedPreviewOffset.Inverse());
	}

	Super::BeginPlay();

	if (const USceneComponent* Root = GetRootComponent())
	{
		ClosedRotation = Root->GetRelativeRotation().Quaternion();
	}
}

bool AStageDoor::CanInteract_Implementation(const FInteractionContext& InteractionContext) const
{
	return CurrentDoorDirection == EDoorState::None;
}

EInteractionResult AStageDoor::OnInteraction_Implementation(const FInteractionContext& InteractionContext)
{
	if (CurrentDoorDirection != EDoorState::None) return EInteractionResult::Rejected;

	CurrentDoorDirection = CurrentDoorState == EDoorState::None || CurrentDoorState == EDoorState::Close ?
		EDoorState::Open : EDoorState::Close;

	FString Direction = "None";
	switch (CurrentDoorDirection)
	{
		case EDoorState::None:
			break;
		case EDoorState::Open:
			Direction = "Open";
			break;
		case EDoorState::Close:
			Direction = "Close";
			break;
	}

	UE_LOG(LogTemp, Warning, TEXT("Direction Current - %s"), *Direction);

	return EInteractionResult::Completed;
}

void AStageDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CurrentDoorDirection == EDoorState::None) return;

	SetDoorMoveAlpha(DoorMoveAlpha + (CurrentDoorDirection == EDoorState::Open ? 1.f : -1.f)  * (DeltaTime / OpenTime));

	SetActorRelativeRotation(ClosedRotation * GetOpeningOffset(DoorMoveAlpha));

	if (DoorMoveAlpha >= 1.f && CurrentDoorDirection == EDoorState::Open)
	{
		CurrentDoorState = EDoorState::Open;
		CurrentDoorDirection = EDoorState::None;
	}
	else if (DoorMoveAlpha <= 0.f && CurrentDoorDirection == EDoorState::Close)
	{
		CurrentDoorState = EDoorState::Close;
		CurrentDoorDirection = EDoorState::None;
	}
}

#if WITH_EDITOR
void AStageDoor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyName != GET_MEMBER_NAME_CHECKED(AStageDoor, DebugDoorMoveAlpha) && PropertyName != GET_MEMBER_NAME_CHECKED(AStageDoor, RotationOffset)) return;

	USceneComponent* Root = GetRootComponent();
	if (!Root || IsTemplate() || HasActorBegunPlay()) return;

	Root->Modify();
	const FQuat BaseRotation = Root->GetRelativeRotation().Quaternion() * AppliedPreviewOffset.Inverse();
	AppliedPreviewOffset = GetOpeningOffset(FMath::Clamp(DebugDoorMoveAlpha, 0.0f, 1.0f));
	SetActorRelativeRotation(BaseRotation * AppliedPreviewOffset);
}
#endif

FQuat AStageDoor::GetOpeningOffset(float Alpha) const
{
	return FQuat::Slerp(FQuat::Identity, RotationOffset.Quaternion(), Alpha);
}

void AStageDoor::SetDoorMoveAlpha(const float& InAlpha)
{
	DoorMoveAlpha = FMath::Clamp(InAlpha, 0.0f, 1.0f);
}

