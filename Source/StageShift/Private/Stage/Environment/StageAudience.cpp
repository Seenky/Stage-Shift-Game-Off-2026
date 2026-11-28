#include "Stage/Environment/StageAudience.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/Dialogue/DialogueComponent.h"
#include "Components/PoseableMeshComponent.h"
#if WITH_EDITORONLY_DATA
#include "Components/SplineComponent.h"
#endif
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Stage/Core/Data/StageDataAsset.h"
#include "Stage/Core/StageManager.h"
#include "Stage/Environment/StageSoundControl.h"
#include "TwoBoneIK.h"

namespace
{
	const FName AudienceViewerTag(TEXT("AudienceViewer"));
	const FName HeadBone(TEXT("head"));
	const FName SpineBone(TEXT("spine_02"));
	const FName TintParameter(TEXT("Tint"));
	constexpr float PointRaiseTime = .22f;
	constexpr float PointLowerTime = .37f;
	constexpr int32 PointingViewerCount = 5;
	constexpr int32 NoticeScorePenaltyPerSecond = 2;
	constexpr double NoticeTipWidthFraction = .10;
	constexpr float NoticeMurmurVolume = .55f;
	constexpr float CurtainOverdueMurmurVolume = .65f;
	constexpr double MurmurPause = 2.0;
	constexpr double MurmurGesturePeriod = 4.5;
	constexpr float LaughDuration = 1.35f;
	constexpr float ApplauseDuration = 2.55f;
	constexpr double ClapPeriod = .34;
	constexpr float SeatPelvisHeight = 10.f;

	struct FPoseEdit
	{
		const FReferenceSkeleton& Skeleton;
		TArray<FTransform>& Local;

		FTransform InMeshSpace(int32 Index) const
		{
			FTransform Result = FTransform::Identity;
			for (; Index != INDEX_NONE; Index = Skeleton.GetParentIndex(Index)) Result *= Local[Index];
			return Result;
		}

		void SetMeshRotation(int32 Index, const FQuat& Rotation)
		{
			const FQuat Parent = InMeshSpace(Skeleton.GetParentIndex(Index)).GetRotation();
			Local[Index].SetRotation((Parent.Inverse() * Rotation).GetNormalized());
		}

		void Aim(FName Bone, FName Child, const FVector& Direction)
		{
			const int32 Index = Skeleton.FindBoneIndex(Bone);
			const int32 ChildIndex = Skeleton.FindBoneIndex(Child);
			if (Index == INDEX_NONE || ChildIndex == INDEX_NONE) return;
			const FTransform Transform = InMeshSpace(Index);
			const FVector Current = (InMeshSpace(ChildIndex).GetLocation() - Transform.GetLocation()).GetSafeNormal();
			SetMeshRotation(Index, FQuat::FindBetweenNormals(Current, Direction.GetSafeNormal()) * Transform.GetRotation());
		}

		void Rotate(FName Bone, const FQuat& Offset)
		{
			const int32 Index = Skeleton.FindBoneIndex(Bone);
			if (Index != INDEX_NONE) SetMeshRotation(Index, Offset * InMeshSpace(Index).GetRotation());
		}
	};

	void MakeSittingPose(const FReferenceSkeleton& Skeleton, TArray<FTransform>& Local)
	{
		Local = Skeleton.GetRefBonePose();
		FPoseEdit Pose{Skeleton, Local};
		const int32 Pelvis = Skeleton.FindBoneIndex(TEXT("pelvis"));
		if (Pelvis == INDEX_NONE) return;
		const FVector PelvisPosition = Pose.InMeshSpace(Pelvis).GetLocation();
		Local[0].AddToTranslation(FVector(0, 0, SeatPelvisHeight - PelvisPosition.Z));

		Pose.Rotate(TEXT("spine_01"), FQuat(FVector::ForwardVector, FMath::DegreesToRadians(5.f)));
		for (const bool bLeft : {true, false})
		{
			const FName Thigh = bLeft ? TEXT("thigh_l") : TEXT("thigh_r");
			const FName Calf = bLeft ? TEXT("calf_l") : TEXT("calf_r");
			const FName Foot = bLeft ? TEXT("foot_l") : TEXT("foot_r");
			const FName UpperArm = bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r");
			const FName LowerArm = bLeft ? TEXT("lowerarm_l") : TEXT("lowerarm_r");
			const FName Hand = bLeft ? TEXT("hand_l") : TEXT("hand_r");
			const float Side = bLeft ? 1.f : -1.f;
			const int32 FootIndex = Skeleton.FindBoneIndex(Foot);
			const FQuat FootRotation = Pose.InMeshSpace(FootIndex).GetRotation();
			Pose.Aim(Thigh, Calf, FVector(Side * .17f, .96f, -.23f));
			Pose.Aim(Calf, Foot, FVector(0, .20f, -1));
			if (FootIndex != INDEX_NONE) Pose.SetMeshRotation(FootIndex, FootRotation);
			Pose.Aim(UpperArm, LowerArm, FVector(Side * .20f, .21f, -.95f));
			Pose.Aim(LowerArm, Hand, FVector(-Side * .25f, .76f, -.62f));
		}
	}

	void MakePointingPose(const FReferenceSkeleton& Skeleton, TArray<FTransform>& Local, FVector Direction, bool bLeft, float Murmur)
	{
		Direction.Y = FMath::Max(0.35, Direction.Y);
		// Keep the gesture above the seat back in the overhead camera.
		Direction.Z = FMath::Clamp(Direction.Z, 0.40, 0.80);
		Direction.Normalize();
		const FName Upper = bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r");
		const FName Lower = bLeft ? TEXT("lowerarm_l") : TEXT("lowerarm_r");
		const FName Hand = bLeft ? TEXT("hand_l") : TEXT("hand_r");
		FPoseEdit Pose{Skeleton, Local};
		Pose.Aim(Upper, Lower, Direction + FVector(0, 0, -.10));
		Pose.Aim(Lower, Hand, Direction + FVector(0, 0, .16));
		if (Murmur <= 0.f) return;

		const FName FreeUpper = bLeft ? TEXT("upperarm_r") : TEXT("upperarm_l");
		const FName FreeLower = bLeft ? TEXT("lowerarm_r") : TEXT("lowerarm_l");
		const FName FreeHand = bLeft ? TEXT("hand_r") : TEXT("hand_l");
		const float Side = bLeft ? -1.f : 1.f;
		Pose.Aim(FreeUpper, FreeLower, FMath::Lerp(FVector(Side * .20, .21, -.95), FVector(Side * .36, .54, -.73), Murmur));
		Pose.Aim(FreeLower, FreeHand, FMath::Lerp(FVector(-Side * .25, .76, -.62), FVector(Side * .10, .55, .45), Murmur));
	}

	float GestureWeight(double Age, float Duration, float Attack, float Release)
	{
		if (Age < 0 || Age >= Duration) return 0.f;
		const float T = static_cast<float>(Age);
		return FMath::SmoothStep(0.f, Attack, T) * (1.f - FMath::SmoothStep(Duration - Release, Duration, T));
	}

	void ApplyClappingPose(FPoseEdit& Pose, float Weight, double Close)
	{
		for (const bool bLeft : {true, false})
		{
			const int32 UpperIndex = Pose.Skeleton.FindBoneIndex(bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r"));
			const int32 LowerIndex = Pose.Skeleton.FindBoneIndex(bLeft ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
			const int32 HandIndex = Pose.Skeleton.FindBoneIndex(bLeft ? TEXT("hand_l") : TEXT("hand_r"));
			if (UpperIndex == INDEX_NONE || LowerIndex == INDEX_NONE || HandIndex == INDEX_NONE) continue;

			FTransform Upper = Pose.InMeshSpace(UpperIndex);
			FTransform Lower = Pose.InMeshSpace(LowerIndex);
			FTransform Hand = Pose.InMeshSpace(HandIndex);
			const double Side = bLeft ? 1.0 : -1.0;
			// Open past the head silhouette so the clap reads from behind the seats.
			const FVector ClapTarget(Side * FMath::Lerp(33.0, 4.5, Close), FMath::Lerp(14.0, 19.0, Close), FMath::Lerp(52.0, 57.0, Close));
			const FVector Target = FMath::Lerp(Hand.GetLocation(), ClapTarget, Weight);
			const FVector Pole = Upper.GetLocation() + FVector(Side * 24.0, -2.0, -18.0);
			AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, Pole, Target, false, 1.0, 1.0);
			Pose.SetMeshRotation(UpperIndex, Upper.GetRotation());
			Pose.SetMeshRotation(LowerIndex, Lower.GetRotation());
		}
	}
}

AStageAudience::AStageAudience()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("AudienceRoot"));
	NoticeMurmurAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("NoticeMurmurAudio"));
	NoticeMurmurAudio->SetupAttachment(RootComponent);
	NoticeMurmurAudio->SetAutoActivate(false);
	NoticeArea = CreateDefaultSubobject<UBoxComponent>(TEXT("PlayerNoticeArea"));
	NoticeArea->SetupAttachment(RootComponent);
	NoticeArea->SetRelativeLocation(FVector(170, 0, 80));
	NoticeArea->SetBoxExtent(FVector(130, 1100, 155));
	NoticeArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NoticeArea->SetCanEverAffectNavigation(false);
	NoticeArea->SetVisibility(false);
#if WITH_EDITORONLY_DATA
	NoticeOutline = CreateEditorOnlyDefaultSubobject<USplineComponent>(TEXT("PlayerNoticeOutline"));
	if (NoticeOutline)
	{
		NoticeOutline->SetupAttachment(NoticeArea);
		NoticeOutline->SetDrawDebug(true);
	}
#endif
}

void AStageAudience::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
#if WITH_EDITOR
	UpdateNoticeOutline();
#endif
	if (!IsTemplate() && !HasActorBegunPlay()) RebuildViewers();
}

void AStageAudience::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITOR
	UpdateNoticeOutline();
#endif
	RebuildViewers();
}

#if WITH_EDITOR
void AStageAudience::UpdateNoticeOutline()
{
	if (!NoticeOutline) return;

	const FVector Extent = NoticeArea->GetScaledBoxExtent();
	const double TipX = Extent.X + NoticeStageDepth;
	const double TipHalfWidth = Extent.Y * NoticeTipWidthFraction;
	TArray<FVector> Corners = {
	    {-Extent.X, -Extent.Y, 0.0}, {Extent.X, -Extent.Y, 0.0}, {TipX, -TipHalfWidth, 0.0},
	    {TipX, TipHalfWidth, 0.0}, {Extent.X, Extent.Y, 0.0}, {-Extent.X, Extent.Y, 0.0},
	};
	const FTransform& Transform = NoticeArea->GetComponentTransform();
	for (FVector& Corner : Corners) Corner = Transform.TransformPositionNoScale(Corner);
	NoticeOutline->SetSplinePoints(Corners, ESplineCoordinateSpace::World, false);
	for (int32 Index = 0; Index < Corners.Num(); ++Index)
		NoticeOutline->SetSplinePointType(Index, ESplinePointType::Linear, false);
	NoticeOutline->SetClosedLoop(true);
}
#endif

void AStageAudience::RebuildViewers()
{
	TInlineComponentArray<UPoseableMeshComponent*> Existing(this);
	for (UPoseableMeshComponent* Component : Existing)
		if (Component->ComponentHasTag(AudienceViewerTag)) Component->DestroyComponent();
	Viewers.Reset();
	for (const FStageAudienceSeat& Seat : Seats)
	{
		if (!Appearances.IsValidIndex(Seat.Appearance) || !Appearances[Seat.Appearance]) continue;
		USkeletalMesh* Asset = Appearances[Seat.Appearance];
		auto* Mesh = NewObject<UPoseableMeshComponent>(this, NAME_None, RF_Transient);
		Mesh->ComponentTags.Add(AudienceViewerTag);
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetRelativeTransform(Seat.SeatTransform);
		Mesh->AddLocalRotation(FRotator(0, -90, 0));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetSkinnedAssetAndUpdate(Asset);
		Mesh->RegisterComponent();
		for (const auto& [SlotName, Tint] : Seat.MaterialTints)
		{
			const int32 Slot = Mesh->GetMaterialIndex(SlotName);
			if (Slot == INDEX_NONE) continue;
			if (auto* Material = Mesh->CreateDynamicMaterialInstance(Slot))
				Material->SetVectorParameterValue(TintParameter, Material->K2_GetVectorParameterValue(TintParameter) * Tint);
		}
		for (const FName SlotName : Seat.HiddenMaterialSlots)
		{
			const int32 Slot = Mesh->GetMaterialIndex(SlotName);
			if (Slot == INDEX_NONE) continue;
			for (int32 LOD = 0; LOD < Mesh->GetNumLODs(); ++LOD)
				Mesh->ShowMaterialSection(Slot, INDEX_NONE, false, LOD);
		}
		FViewer& Viewer = Viewers.AddDefaulted_GetRef();
		Viewer.Mesh = Mesh;
		Viewer.LookPoint = GetActorTransform().TransformPosition(StageFocus);
		MakeSittingPose(Asset->GetRefSkeleton(), Viewer.SittingPose);
		Mesh->BoneSpaceTransforms = Viewer.SittingPose;
		Mesh->RefreshBoneTransforms();
	}
}

void AStageAudience::BeginPlay()
{
	Super::BeginPlay();
	if (Viewers.IsEmpty()) RebuildViewers();
	NoticeMurmurAudio->AttenuationSettings = LaughAttenuation;
	StageManager = Cast<AStageManager>(UGameplayStatics::GetActorOfClass(this, AStageManager::StaticClass()));
	if (StageManager.IsValid()) StageManager->OnScoreChanged.AddUObject(this, &ThisClass::HandleScoreChanged);
	SoundControl = Cast<AStageSoundControl>(UGameplayStatics::GetActorOfClass(this, AStageSoundControl::StaticClass()));
	if (SoundControl.IsValid()) SoundControl->OnCueResult.AddUniqueDynamic(this, &ThisClass::HandleSoundResult);
}

void AStageAudience::EndPlay(const EEndPlayReason::Type Reason)
{
	if (SoundControl.IsValid()) SoundControl->OnCueResult.RemoveDynamic(this, &ThisClass::HandleSoundResult);
	if (StageManager.IsValid()) StageManager->OnScoreChanged.RemoveAll(this);
	NoticeMurmurAudio->Stop();
	Super::EndPlay(Reason);
}

bool AStageAudience::IsScenePlaying() const
{
	return StageManager.IsValid() && (StageManager->GetState() == EStageState::Running || StageManager->GetState() == EStageState::Finalizing);
}

bool AStageAudience::IsInNoticeArea(const FVector& WorldLocation) const
{
	const FVector Point = NoticeArea->GetComponentTransform().InverseTransformPositionNoScale(WorldLocation);
	const FVector Extent = NoticeArea->GetScaledBoxExtent();
	const double SideDistance = FMath::Abs(Point.Y);
	if (FMath::Abs(Point.Z) > Extent.Z || SideDistance > Extent.Y || Point.X < -Extent.X) return false;

	const double Depth = FMath::GetMappedRangeValueClamped(
	    FVector2D(Extent.Y * NoticeTipWidthFraction, Extent.Y), FVector2D(NoticeStageDepth, 0.0), SideDistance);
	return Point.X <= Extent.X + Depth;
}

void AStageAudience::UpdateFocus()
{
	const UStageDataAsset* Stage = IsScenePlaying() ? StageManager->GetActiveStage() : nullptr;
	AActor* Next = Stage ? Speaker.Get() : nullptr;
	bDialogueRevealing = false;
	if (Stage)
		for (FName RoleId : Stage->GetRoleOptions())
		{
			AActor* Performer = StageManager->GetParticipant(RoleId);
			const auto* Dialogue = Performer ? Performer->FindComponentByClass<UDialogueComponent>() : nullptr;
			if (!Dialogue || !Dialogue->IsPlaying()) continue;
			Next = Performer;
			if (Dialogue->GetPlaybackState().Phase == EDialoguePlaybackPhase::Revealing)
			{
				bDialogueRevealing = Dialogue->HasDialogueVoice();
				break;
			}
		}
	if (Speaker == Next) return;
	Speaker = Next;
	FocusChangedAt = GetWorld()->GetTimeSeconds();
}

void AStageAudience::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Viewers.IsEmpty()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextFocusCheck)
	{
		NextFocusCheck = Now + .10;
		UpdateFocus();
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bScenePlaying = IsScenePlaying();
	if (!bScenePlaying) PlayerNoticeSeconds = 0.0;

	TArray<TPair<int32, double>, TInlineAllocator<16>> Nearest;
	if (bScenePlaying && Player && IsInNoticeArea(Player->GetActorLocation()))
	{
		PlayerNoticeSeconds += DeltaSeconds;
		const int32 WholeSeconds = FMath::FloorToInt(PlayerNoticeSeconds);
		if (WholeSeconds > 0)
		{
			PlayerNoticeSeconds -= WholeSeconds;
			StageManager->AddScore(-NoticeScorePenaltyPerSecond * WholeSeconds);
		}

		const FVector PlayerLocation = Player->GetActorLocation();
		for (int32 Index = 0; Index < Viewers.Num(); ++Index)
		{
			if (const UPoseableMeshComponent* Mesh = Viewers[Index].Mesh.Get())
				Nearest.Emplace(Index, FVector::DistSquared2D(Mesh->GetComponentLocation(), PlayerLocation));
		}
		Nearest.Sort([](const auto& A, const auto& B) { return A.Value != B.Value ? A.Value < B.Value : A.Key < B.Key; });
		Nearest.SetNum(FMath::Min(Nearest.Num(), PointingViewerCount));
	}
	const bool bPlayerNoticed = !Nearest.IsEmpty();
	const bool bNewNotice = bPlayerNoticed && PlayerNoticedAt < 0.0;
	if (bNewNotice)
	{
		PlayerNoticedAt = Now;
		StageManager->OnPlayerNoticedByAudience.Broadcast();
	}
	else if (!bPlayerNoticed) PlayerNoticedAt = -1.0;

	const bool bOpeningOverdue = StageManager.IsValid() && StageManager->IsCurtainOpeningOverdue();
	const bool bNewOverdue = bOpeningOverdue && !bWasOpeningOverdue;
	bWasOpeningOverdue = bOpeningOverdue;
	const float Gain = FMath::FInterpTo(MurmurDialogueGain, bDialogueRevealing ? .56f : 1.f, DeltaSeconds, bDialogueRevealing ? 12.f : 6.f);
	if (Gain != MurmurDialogueGain)
	{
		MurmurDialogueGain = Gain;
		NoticeMurmurAudio->SetVolumeMultiplier(Gain);
	}
	if ((bNewNotice || bNewOverdue) && Now >= NextMurmurAt && !NoticeMurmurAudio->IsPlaying())
	{
		USoundBase* Sound = bNewOverdue && CurtainOverdueMurmurSound ? CurtainOverdueMurmurSound.Get() : NoticeMurmurSound.Get();
		if (Sound && !Sound->IsLooping())
		{
			FVector Center = GetActorLocation();
			if (bNewNotice && !bNewOverdue)
			{
				Center = FVector::ZeroVector;
				for (const auto& Seat : Nearest) Center += Viewers[Seat.Key].Mesh->GetComponentLocation();
				Center /= Nearest.Num();
			}
			Center.Z += 68.0;
			NoticeMurmurAudio->SetWorldLocation(Center);
			NoticeMurmurAudio->SetSound(Sound);
			NoticeMurmurAudio->FadeIn(.05f, bNewOverdue ? CurtainOverdueMurmurVolume : NoticeMurmurVolume);
			NextMurmurAt = Now + Sound->GetDuration() + MurmurPause;
		}
	}
	for (int32 Index = 0; Index < Viewers.Num(); ++Index)
	{
		const bool bPointing = Nearest.ContainsByPredicate([Index](const auto& Seat) { return Seat.Key == Index; });
		UpdateViewer(Viewers[Index], Index, DeltaSeconds, Now, bPointing ? Player : nullptr);
	}
}

void AStageAudience::UpdateViewer(FViewer& Viewer, int32 Index, float DeltaSeconds, double Now, const APawn* PointingTarget)
{
	UPoseableMeshComponent* Mesh = Viewer.Mesh.Get();
	if (!Mesh) return;
	const auto* Asset = Cast<USkeletalMesh>(Mesh->GetSkinnedAsset());
	if (!Asset) return;
	const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
	const bool bWasPointing = Viewer.PointAlpha > 0.f;
	const bool bPointing = PointingTarget != nullptr;
	const float BlendTime = bPointing ? PointRaiseTime : PointLowerTime;
	Viewer.PointAlpha = FMath::FInterpConstantTo(Viewer.PointAlpha, bPointing ? 1.f : 0.f, DeltaSeconds, 1.f / BlendTime);
	const float Point = FMath::SmoothStep(0.f, 1.f, Viewer.PointAlpha);
	const float Laugh = GestureWeight(Now - Viewer.LaughAt, LaughDuration, .16f, .34f);
	const double ClapAge = Now - ApplauseAt - (Index % 5) * .055;
	const float Clapping = GestureWeight(ClapAge, ApplauseDuration, .22f, .35f) * (1.f - Point);
	if (Now >= FocusChangedAt + (Index % 5) * .063)
		Viewer.LookPoint = Speaker.IsValid() ? Speaker->GetActorLocation() + FVector(0, 0, 55) : GetActorTransform().TransformPosition(StageFocus);
	FVector Target = Viewer.LookPoint;
	if (PointingTarget)
	{
		const FVector PlayerFocus = PointingTarget->GetActorLocation() + FVector(0, 0, 40);
		const FVector Aim = Mesh->GetComponentTransform().InverseTransformPosition(PlayerFocus) - FVector(0, 0, 40);
		if (!bWasPointing) Viewer.bPointWithLeftArm = Aim.X > 0.0;
		const double MurmurAge = FMath::Fmod(Now - PlayerNoticedAt - (Index % 5) * .07, MurmurGesturePeriod);
		const float Murmur = GestureWeight(MurmurAge, 1.55f, .2f, .42f);
		Viewer.PointingPose = Viewer.SittingPose;
		MakePointingPose(Skeleton, Viewer.PointingPose, Aim.GetSafeNormal(), Viewer.bPointWithLeftArm, Murmur);
		Target = FMath::Lerp(Viewer.LookPoint, PlayerFocus, Point);
	}
	const FVector Local = Mesh->GetComponentTransform().InverseTransformPosition(Target) - FVector(0, 0, 52);
	const float MaxYaw = FMath::Lerp(36.f, 65.f, Point);
	const float Yaw = FMath::Clamp(static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-Local.X, Local.Y))), -MaxYaw, MaxYaw);
	const float Pitch = FMath::Clamp(static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Local.Z, Local.Size2D()))), -14.f, 18.f);
	const float NewYaw = FMath::FInterpTo(Viewer.HeadYaw, Yaw, DeltaSeconds, 5.f);
	const float NewPitch = FMath::FInterpTo(Viewer.HeadPitch, Pitch, DeltaSeconds, 4.f);
	const bool bSettled = FMath::IsNearlyEqual(Viewer.HeadYaw, NewYaw, .02f) && FMath::IsNearlyEqual(Viewer.HeadPitch, NewPitch, .02f);
	Viewer.HeadYaw = NewYaw;
	Viewer.HeadPitch = NewPitch;
	// Restore the seated pose once when a gesture ends, then stop refreshing until focus moves.
	const bool bRecentGesture = bPointing || bWasPointing || Now - Viewer.LaughAt < LaughDuration + .12
	    || ClapAge < ApplauseDuration + .12;
	if (bSettled && !bRecentGesture) return;

	auto& LocalPose = Mesh->BoneSpaceTransforms;
	LocalPose = Viewer.SittingPose;
	if (Point > 0.f && Viewer.PointingPose.Num() == LocalPose.Num())
		for (int32 Bone = 0; Bone < LocalPose.Num(); ++Bone)
			LocalPose[Bone].Blend(Viewer.SittingPose[Bone], Viewer.PointingPose[Bone], Point);
	FPoseEdit Pose{Skeleton, LocalPose};
	if (Clapping > 0.f)
	{
		const double Close = .5 - .5 * FMath::Cos(2.0 * UE_DOUBLE_PI * ClapAge / ClapPeriod);
		ApplyClappingPose(Pose, Clapping, Close);
	}
	const float Chuckle = Laugh * (5.f + 3.f * FMath::Sin(static_cast<float>((Now - Viewer.LaughAt) * 25.0)));
	Pose.Rotate(SpineBone, FQuat(FVector::UpVector, FMath::DegreesToRadians(NewYaw * Point * .22f))
	    * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-Chuckle - Point * 6.f)));
	Pose.Rotate(HeadBone, FQuat(FVector::UpVector, FMath::DegreesToRadians(NewYaw))
	    * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(NewPitch + Chuckle * .4f)));
	Mesh->MarkRefreshTransformDirty();
}

void AStageAudience::HandleScoreChanged(int32 Amount)
{
	if (Amount <= 0 || !IsScenePlaying() || Viewers.IsEmpty()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < ApplauseAt + ApplauseDuration + .5) return;

	ApplauseAt = Now;
	if (ApplauseSound) UGameplayStatics::PlaySoundAtLocation(this, ApplauseSound, GetActorLocation() + FVector(0, 0, 68),
	    .75f, 1.f, 0.f, LaughAttenuation);
}

void AStageAudience::HandleSoundResult(const FStageSoundWindow& Window, EStageSoundResult Result)
{
	if (bLaughAtWrongSound && Result == EStageSoundResult::WrongSound && IsScenePlaying()) ReactWithLaughter();
}

void AStageAudience::ReactWithLaughter()
{
	if (!HasActorBegunPlay() || Viewers.IsEmpty()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextLaughAt) return;
	NextLaughAt = Now + 3.4;
	for (int32 Index = 0; Index < Viewers.Num(); ++Index)
		if (Index % 3 == 1) Viewers[Index].LaughAt = Now + .10 * (Index / 3);
	if (LaughSound) UGameplayStatics::PlaySoundAtLocation(this, LaughSound, GetActorLocation() + FVector(0, 0, 68),
	    .7f, 1.f, 0.f, LaughAttenuation);
}
