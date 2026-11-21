#include "Stage/Environment/StageSoundControl.h"

#include "Blueprint/UserWidget.h"
#include "Components/Dialogue/DialogueComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Components/Stage/StageMemberComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogStageSound, Log, All);

float FStageSoundTiming::GetDuration() const
{
	return FMath::Max(0.f, ExpectedDelay) + FMath::Max(0.f, LateTolerance) + FMath::Max(0.01f, LateGrace);
}

AStageSoundControl::AStageSoundControl()
{
	auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Interaction"));
	SetRootComponent(Root);

	ControlMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ControlMesh"));
	ControlMesh->SetupAttachment(Root);
	ControlMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	SoundSelector = CreateDefaultSubobject<UWidgetComponent>(TEXT("SoundSelector"));
	SoundSelector->SetupAttachment(Root);
	SoundSelector->SetRelativeLocation(FVector(0, 0, 205));
	SoundSelector->SetWidgetSpace(EWidgetSpace::Screen);
	SoundSelector->SetDrawAtDesiredSize(true);
	SoundSelector->SetPivot(FVector2D(0.5, 1.0));
	SoundSelector->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SoundSelector->SetVisibility(false);
}

const UMeshComponent* AStageSoundControl::GetInteractionMesh() const
{
	return ControlMesh.Get();
}

bool AStageSoundControl::CanInteract_Implementation(const FInteractionContext& Context) const
{
	if (const auto* Component = Context.Hit.GetComponent(); Component && Component != ControlMesh.Get()) return false;
	const auto* Pawn = Cast<APawn>(Context.Interactor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		if (!SoundOptions.IsEmpty()) return HasSound(GetSelectedSoundId());
		return IsValid(Sound) && GetWorld() && GetWorld()->GetTimeSeconds() >= NextAttemptAt && (Window.bActive || bCanArmFromConsole);
	}
	return IsValid(Context.Interactor) && Context.Interactor->FindComponentByClass<UStageMemberComponent>() && !Window.bActive;
}

EInteractionResult AStageSoundControl::OnInteraction_Implementation(const FInteractionContext& Context)
{
	if (Context.InteractionPhase != EInteractionPhase::Start || !CanInteract_Implementation(Context)) return EInteractionResult::Rejected;
	const auto* Pawn = Cast<APawn>(Context.Interactor);
	auto* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	bool bApplied;
	if (Player && Window.bActive) bApplied = PlaySignal();
	else if (Player && !SoundOptions.IsEmpty()) bApplied = PreviewSelectedSound(*Player);
	else bApplied = OpenWindow();
	return bApplied ? EInteractionResult::Completed : EInteractionResult::Rejected;
}

bool AStageSoundControl::PreviewSelectedSound(APlayerController& Player)
{
	if (!Player.IsLocalController() || !GetWorld() || !SoundOptions.IsValidIndex(SelectedSoundIndex)) return false;
	USoundBase* Playback = SoundOptions[SelectedSoundIndex].Sound.Get();
	const float Now = GetWorld()->GetTimeSeconds();
	if (!IsValid(Playback) || Now < NextPreviewAt) return false;
	const float Duration = Playback->GetDuration();
	NextPreviewAt = Now + (FMath::IsFinite(Duration) ? FMath::Max(0.35f, Duration) : 0.35f);
	UGameplayStatics::PlaySound2D(&Player, Playback);
	return true;
}

bool AStageSoundControl::OpenWindow()
{
	if (!GetWorld() || !IsValid(Sound)) return false;
	AActor* Target = nullptr;
	if (!SoundTarget.IsNull())
	{
		FSoftObjectPath Path = SoundTarget.ToSoftObjectPath();
		if (GetWorld()->WorldType == EWorldType::PIE) Path.FixupForPIE(GetWorld()->GetPackage()->GetPIEInstanceID());
		Target = Cast<AActor>(Path.ResolveObject());
	}
	return StartWindow(CueId, NAME_None, Target, GetLegacyTiming());
}

bool AStageSoundControl::OpenSoundWindow(FName SoundId, AActor* Target, const FStageSoundTiming& Timing)
{
	return HasSound(SoundId) && StartWindow(SoundId, SoundId, Target, Timing);
}

bool AStageSoundControl::StartWindow(FName InCueId, FName SoundId, AActor* Target, const FStageSoundTiming& Timing)
{
	UWorld* World = GetWorld();
	if (!World || !HasActorBegunPlay() || IsActorBeingDestroyed() || Window.bActive) return false;
	Window = {};
	Window.Id = ++LastWindowId;
	Window.CueId = InCueId;
	Window.SoundId = SoundId;
	Window.Target = Target;
	Window.bActive = true;
	WindowTiming = Timing;
	ReportedResults = 0;
	SetTiming(World->GetTimeSeconds());
	BroadcastResult(EStageSoundResult::Ready);
	return true;
}

float AStageSoundControl::GetWindowDuration() const
{
	return GetLegacyTiming().GetDuration();
}

FStageSoundTiming AStageSoundControl::GetLegacyTiming() const
{
	return {ExpectedDelay, EarlyTolerance, LateTolerance, LateGrace, MaxAttempts, RetryDelay, bRetryLate};
}

bool AStageSoundControl::HasSound(FName SoundId) const
{
	return !SoundId.IsNone() && SoundOptions.ContainsByPredicate([SoundId](const FStageSoundOption& Option)
	{
		return Option.SoundId == SoundId && IsValid(Option.Sound);
	});
}

FName AStageSoundControl::GetSelectedSoundId() const
{
	return SoundOptions.IsValidIndex(SelectedSoundIndex) ? SoundOptions[SelectedSoundIndex].SoundId : NAME_None;
}

void AStageSoundControl::SetSelectorFocused(bool bFocused)
{
	const bool bShow = bFocused && !SoundOptions.IsEmpty() && SoundSelector->GetWidgetClass();
	SoundSelector->SetVisibility(bShow);
	if (bShow) RefreshSelector();
}

void AStageSoundControl::CycleSound(int32 Direction)
{
	if (SoundOptions.Num() <= 1 || Direction == 0) return;
	SelectedSoundIndex = (SelectedSoundIndex + FMath::Sign(Direction) + SoundOptions.Num()) % SoundOptions.Num();
	RefreshSelector();
}

void AStageSoundControl::RefreshSelector()
{
	SoundSelector->InitWidget();
	UUserWidget* Widget = SoundSelector->GetUserWidgetObject();
	if (!Widget || !SoundOptions.IsValidIndex(SelectedSoundIndex)) return;

	const auto SetLabel = [&](FName WidgetName, int32 Index)
	{
		if (auto* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(WidgetName)))
		{
			const FStageSoundOption& Option = SoundOptions[Index];
			Label->SetText(Option.DisplayName.IsEmpty() ? FText::FromName(Option.SoundId) : Option.DisplayName);
		}
	};
	SetLabel(TEXT("SelectedSound"), SelectedSoundIndex);
	if (SoundOptions.Num() > 1) SetLabel(TEXT("PreviousSound"), (SelectedSoundIndex + SoundOptions.Num() - 1) % SoundOptions.Num());
	if (SoundOptions.Num() > 2) SetLabel(TEXT("NextSound"), (SelectedSoundIndex + 1) % SoundOptions.Num());
}

void AStageSoundControl::SetTiming(float OpenedAt)
{
	Window.OpenedAt = OpenedAt;
	const float ExpectedAt = Window.OpenedAt + FMath::Max(0.f, WindowTiming.ExpectedDelay);
	Window.EarliestAt = FMath::Max(Window.OpenedAt, ExpectedAt - FMath::Max(0.f, WindowTiming.EarlyTolerance));
	Window.LatestAt = ExpectedAt + FMath::Max(0.f, WindowTiming.LateTolerance);
	Window.ExpiresAt = Window.OpenedAt + WindowTiming.GetDuration();
	GetWorld()->GetTimerManager().SetTimer(WindowTimer, this, &ThisClass::ExpireWindow, Window.ExpiresAt - Window.OpenedAt, false);
}

bool AStageSoundControl::PlaySignal()
{
	if (!Window.bActive || !GetWorld()) return false;
	const bool bUseSelector = !Window.SoundId.IsNone();
	if (bUseSelector && !SoundOptions.IsValidIndex(SelectedSoundIndex)) return false;
	const FStageSoundOption* Selected = bUseSelector ? &SoundOptions[SelectedSoundIndex] : nullptr;
	USoundBase* Playback = Selected ? Selected->Sound.Get() : Sound.Get();
	if (!IsValid(Playback)) return false;
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= Window.ExpiresAt)
	{
		ExpireWindow();
		return false;
	}
	if (Now < NextAttemptAt) return false;
	const EStageSoundResult Result = Selected && Selected->SoundId != Window.SoundId ? EStageSoundResult::WrongSound
	    : Now < Window.EarliestAt ? EStageSoundResult::Early
	    : Now <= Window.LatestAt ? EStageSoundResult::OnTime : EStageSoundResult::Late;
	++Window.Attempt;
	const float SoundDuration = Playback->GetDuration();
	const float PlaybackCooldown = FMath::IsFinite(SoundDuration) ? FMath::Max(0.f, SoundDuration) : 0.f;
	NextAttemptAt = Now + FMath::Max3(0.1f, WindowTiming.RetryDelay, PlaybackCooldown);
	const bool bHasAttemptLeft = Window.Attempt < FMath::Max(1, WindowTiming.MaxAttempts);
	const bool bRearmLate = Result == EStageSoundResult::Late && WindowTiming.bRetryLate && bHasAttemptLeft;
	if (Result == EStageSoundResult::OnTime || (Result == EStageSoundResult::Late && !bRearmLate) || !bHasAttemptLeft) CloseWindow();
	UGameplayStatics::PlaySoundAtLocation(this, Playback, IsValid(Window.Target) ? Window.Target->GetActorLocation() : GetActorLocation());
	const uint32 WindowId = Window.Id;
	const int32 Attempt = Window.Attempt;
	BroadcastResult(Result);
	if (bRearmLate && Window.bActive && Window.Id == WindowId && Window.Attempt == Attempt)
	{
		SetTiming(Now);
		ReportedResults &= ~(1u << static_cast<uint8>(EStageSoundResult::Ready));
		BroadcastResult(EStageSoundResult::Ready);
	}
	return true;
}

void AStageSoundControl::BroadcastResult(EStageSoundResult Result)
{
	LastResult = Result;
	const uint8 Bit = 1u << static_cast<uint8>(Result);
	const FStageSoundWindow Snapshot = Window;
	const bool bPlayed = Result == EStageSoundResult::Early || Result == EStageSoundResult::OnTime
	    || Result == EStageSoundResult::Late || Result == EStageSoundResult::WrongSound;
	const FName PlayedSound = bPlayed ? (Snapshot.SoundId.IsNone() ? Snapshot.CueId : GetSelectedSoundId()) : NAME_None;
	UE_LOG(LogStageSound, Log, TEXT("%s / Window[%u] / Cue[%s] / Expected[%s] / Played[%s] / Result=%s / Attempt=%d / Elapsed=%.2fs / OnTime=%.2f-%.2fs"),
	    *GetName(), Snapshot.Id, *Snapshot.CueId.ToString(), *Snapshot.SoundId.ToString(), *PlayedSound.ToString(),
	    *UEnum::GetValueAsString(Result), Snapshot.Attempt, GetWorld()->GetTimeSeconds() - Snapshot.OpenedAt,
	    Snapshot.EarliestAt - Snapshot.OpenedAt, Snapshot.LatestAt - Snapshot.OpenedAt);
	if (!(ReportedResults & Bit))
	{
		ReportedResults |= Bit;
		if (IsValid(Snapshot.Target))
			if (const FDialogueLine* Line = TargetResponseLines.Find(Result); Line && !Line->Text.IsEmpty())
				if (auto* Dialogue = Snapshot.Target->FindComponentByClass<UDialogueComponent>()) Dialogue->PlayLine(*Line, true);
	}
	OnCueResult.Broadcast(Snapshot, Result);
}

void AStageSoundControl::ExpireWindow()
{
	if (!Window.bActive) return;
	CloseWindow();
	BroadcastResult(EStageSoundResult::Missed);
}

void AStageSoundControl::CancelWindow()
{
	if (!Window.bActive) return;
	CloseWindow();
	BroadcastResult(EStageSoundResult::Cancelled);
}

void AStageSoundControl::CloseWindow()
{
	Window.bActive = false;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(WindowTimer);
}

void AStageSoundControl::EndPlay(const EEndPlayReason::Type Reason)
{
	CloseWindow();
	Window.Target = nullptr;
	Super::EndPlay(Reason);
}
