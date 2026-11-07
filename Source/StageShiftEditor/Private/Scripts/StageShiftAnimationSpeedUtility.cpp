#include "Scripts/StageShiftAnimationSpeedUtility.h"

#include "AnimPose.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Curves/RichCurve.h"
#include "EditorUtilityLibrary.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "StageShiftAnimationSpeed"

DEFINE_LOG_CATEGORY_STATIC(LogStageShiftAnimationSpeed, Log, All);

namespace
{
	struct FLocomotionSample
	{
		FVector Forward = FVector(0.0, 1.0, 0.0);
		float Direction = 0.0f;
		float Gait = 0.0f;
	};

	FLocomotionSample GetLocomotionSample(const FString& Name)
	{
		FLocomotionSample Sample;
		Sample.Gait = Name.Contains(TEXT("Run")) ? 2.0f : Name.Contains(TEXT("Walk")) ? 1.0f : 0.0f;

		// The Stage Shift FBXs face +Y in skeletal mesh space.
		if (Name.EndsWith(TEXT("90L")))
		{
			Sample.Forward = FVector(1.0, 0.0, 0.0);
			Sample.Direction = -90.0f;
		}
		else if (Name.EndsWith(TEXT("90R")))
		{
			Sample.Forward = FVector(-1.0, 0.0, 0.0);
			Sample.Direction = 90.0f;
		}
		else if (Name.EndsWith(TEXT("Bwd")))
		{
			Sample.Forward = FVector(0.0, -1.0, 0.0);
			Sample.Direction = 180.0f;
		}
		return Sample;
	}

	TArray<double> GetStanceSpeeds(TConstArrayView<FVector> AnklePositions, const FVector& Forward, double SecondsPerFrame)
	{
		if (AnklePositions.Num() < 2)
		{
			return {};
		}

		double MinHeight = AnklePositions[0].Z;
		double MaxHeight = MinHeight;
		for (const FVector& Position : AnklePositions)
		{
			MinHeight = FMath::Min(MinHeight, Position.Z);
			MaxHeight = FMath::Max(MaxHeight, Position.Z);
		}

		const double ContactHeight = MinHeight + FMath::Max(0.5, (MaxHeight - MinHeight) * 0.25);
		TArray<double> Speeds;
		for (int32 Index = 1; Index < AnklePositions.Num(); ++Index)
		{
			const FVector& Previous = AnklePositions[Index - 1];
			const FVector& Current = AnklePositions[Index];
			if ((Previous.Z + Current.Z) * 0.5 <= ContactHeight)
			{
				const double Speed = FVector::DotProduct(Previous - Current, Forward) / SecondsPerFrame;
				if (Speed >= 1.0)
				{
					Speeds.Add(Speed);
				}
			}
		}
		return Speeds;
	}

	struct FFootSamples
	{
		TArray<FVector> Ankles;
		TArray<double> Heights;
	};

	double SampleFeet(const UAnimSequence& Animation, const FReferenceSkeleton& Skeleton, FFootSamples (&Samples)[2])
	{
		const FName Root = Skeleton.GetBoneName(0);
		const FName Feet[] = {TEXT("foot_l"), TEXT("foot_r")};
		const FName Balls[] = {TEXT("ball_l"), TEXT("ball_r")};
		const bool bHasFoot[] = {Skeleton.FindBoneIndex(Feet[0]) != INDEX_NONE, Skeleton.FindBoneIndex(Feet[1]) != INDEX_NONE};
		const bool bHasBall[] = {Skeleton.FindBoneIndex(Balls[0]) != INDEX_NONE, Skeleton.FindBoneIndex(Balls[1]) != INDEX_NONE};
		const int32 NumFrames = Animation.GetNumberOfSampledKeys();
		FAnimPoseEvaluationOptions Options;
		Options.bShouldRetarget = false;
		Options.bEvaluateCurves = false;

		FVector PreviousRoot = FVector::ZeroVector;
		double RootDistance = 0.0;
		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			FAnimPose Pose;
			UAnimPoseExtensions::GetAnimPoseAtFrame(&Animation, Frame, Options, Pose);
			const FVector CurrentRoot = UAnimPoseExtensions::GetBonePose(Pose, Root, EAnimPoseSpaces::World).GetTranslation();
			if (Frame > 0)
			{
				RootDistance += FVector::Dist2D(CurrentRoot, PreviousRoot);
			}
			PreviousRoot = CurrentRoot;

			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (bHasFoot[Side])
				{
					const FVector Ankle = UAnimPoseExtensions::GetBonePose(Pose, Feet[Side], EAnimPoseSpaces::World).GetTranslation();
					const double ToeHeight = bHasBall[Side]
						? UAnimPoseExtensions::GetBonePose(Pose, Balls[Side], EAnimPoseSpaces::World).GetLocation().Z
						: Ankle.Z;
					Samples[Side].Ankles.Add(Ankle);
					Samples[Side].Heights.Add(ToeHeight - CurrentRoot.Z);
				}
			}
		}
		return RootDistance;
	}

	double EstimateSpeed(const FFootSamples (&Samples)[2], const FVector& Forward, double Duration, double RootDistance)
	{
		if (RootDistance >= 0.5)
		{
			return RootDistance / Duration;
		}

		const int32 NumFrames = FMath::Max(Samples[0].Ankles.Num(), Samples[1].Ankles.Num());
		if (NumFrames < 2)
		{
			return 0.0;
		}
		const double SecondsPerFrame = Duration / (NumFrames - 1);
		TArray<double> Speeds = GetStanceSpeeds(Samples[0].Ankles, Forward, SecondsPerFrame);
		Speeds.Append(GetStanceSpeeds(Samples[1].Ankles, Forward, SecondsPerFrame));
		if (Speeds.IsEmpty())
		{
			return 0.0;
		}
		Speeds.Sort();
		return (Speeds[(Speeds.Num() - 1) / 2] + Speeds[Speeds.Num() / 2]) * 0.5;
	}

	TArray<FRichCurveKey> MakeContactKeys(const FFootSamples& Foot, const FVector& Forward, float Duration,
		double Speed, bool bHasRootMotion, bool bIdle)
	{
		if (bIdle)
		{
			return {FRichCurveKey(0.0f, 1.0f), FRichCurveKey(Duration, 1.0f)};
		}
		if (Foot.Heights.Num() < 2)
		{
			return {};
		}

		const double MinHeight = FMath::Min(Foot.Heights);
		const double Range = FMath::Max(1.5, (FMath::Max(Foot.Heights) - MinHeight) * 0.4);
		const int32 Last = Foot.Heights.Num() - 1;
		const double FrameTime = Duration / Last;
		const bool bLoop = !bHasRootMotion && Foot.Ankles[0].Equals(Foot.Ankles[Last], 0.5);
		TArray<FRichCurveKey> Keys;
		Keys.Reserve(Last + 1);
		for (int32 Frame = 0; Frame <= Last; ++Frame)
		{
			const double Height = Foot.Heights[Frame] - MinHeight;
			double Contact = 1.0 - FMath::SmoothStep(Range * 0.45, Range, Height);
			if (!bHasRootMotion && Speed >= 1.0)
			{
				const int32 Previous = Frame > 0 ? Frame - 1 : (bLoop ? Last - 1 : 0);
				const int32 Next = Frame < Last ? Frame + 1 : (bLoop ? 1 : Last);
				const double SampleTime = FrameTime * (bLoop || (Frame > 0 && Frame < Last) ? 2.0 : 1.0);
				const double BackwardSpeed = FVector::DotProduct(Foot.Ankles[Previous] - Foot.Ankles[Next], Forward) / SampleTime;
				Contact *= FMath::SmoothStep(-0.35 * Speed, 0.15 * Speed, BackwardSpeed);
			}
			FRichCurveKey& Key = Keys.Emplace_GetRef(Frame == Last ? Duration : static_cast<float>(FrameTime * Frame),
				static_cast<float>(Contact));
			Key.InterpMode = RCIM_Linear;
		}
		if (bLoop)
		{
			Keys.Last().Value = Keys[0].Value;
		}
		return Keys;
	}

	bool WriteCurve(IAnimationDataController& Controller, const IAnimationDataModel& Model, FName Name, const TArray<FRichCurveKey>& Keys)
	{
		const FAnimationCurveIdentifier Curve(Name, ERawCurveTrackTypes::RCT_Float);
		return (Model.FindFloatCurve(Curve) || Controller.AddCurve(Curve)) && Controller.SetCurveKeys(Curve, Keys);
	}

	bool BakeCurves(UAnimSequence* Animation, bool bContactsOnly)
	{
		const USkeleton* Skeleton = Animation ? Animation->GetSkeleton() : nullptr;
		const IAnimationDataModel* Model = Animation ? Animation->GetDataModel() : nullptr;
		if (!Skeleton || !Model || Skeleton->GetReferenceSkeleton().GetNum() == 0 ||
			Animation->GetNumberOfSampledKeys() < 2 || Animation->GetPlayLength() <= UE_SMALL_NUMBER ||
			Animation->IsValidAdditive())
		{
			UE_LOG(LogStageShiftAnimationSpeed, Warning, TEXT("Skipping %s. Expected a sampled, non-additive animation with a skeleton."), *GetNameSafe(Animation));
			return false;
		}

		const FLocomotionSample Sample = GetLocomotionSample(Animation->GetName());
		const bool bIdle = Animation->GetName().Contains(TEXT("Idle"));
		const bool bBakeContacts = bIdle || Sample.Gait > 0.0f;
		if (bContactsOnly && !bBakeContacts)
		{
			return false;
		}

		FFootSamples Feet[2];
		const float Duration = Animation->GetPlayLength();
		const double RootDistance = SampleFeet(*Animation, Skeleton->GetReferenceSkeleton(), Feet);
		const float Speed = static_cast<float>(EstimateSpeed(Feet, Sample.Forward, Duration, RootDistance));

		IAnimationDataController& Controller = Animation->GetController();
		IAnimationDataController::FScopedBracket Bracket(Controller, LOCTEXT("BakeSpeed", "Bake locomotion curves"));
		Animation->Modify();
		if (!bContactsOnly)
		{
			const TPair<FName, float> Values[] = {
				{TEXT("Speed_Data"), Speed},
				{TEXT("PlayRate"), Animation->RateScale},
				{TEXT("Gait"), Sample.Gait},
				{TEXT("Stride"), Sample.Gait * 0.5f},
				{TEXT("Direction"), Sample.Direction},
			};
			for (const TPair<FName, float>& Value : Values)
			{
				if (!WriteCurve(Controller, *Model, Value.Key, {FRichCurveKey(0.0f, Value.Value), FRichCurveKey(Duration, Value.Value)}))
				{
					return false;
				}
			}
		}

		if (bBakeContacts)
		{
			const FName CurveNames[] = {TEXT("FootPlant_L"), TEXT("FootPlant_R")};
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const TArray<FRichCurveKey> Keys = MakeContactKeys(Feet[Side], Sample.Forward, Duration, Speed, RootDistance >= 0.5, bIdle);
				if (!Keys.IsEmpty() && !WriteCurve(Controller, *Model, CurveNames[Side], Keys))
				{
					return false;
				}
			}
		}

		Animation->MarkPackageDirty();
		UE_LOG(LogStageShiftAnimationSpeed, Log, TEXT("%s: %.2f cm/s at 1x"), *GetNameSafe(Animation), Speed);
		return true;
	}
}


UStageShiftAnimationSpeedUtility::UStageShiftAnimationSpeedUtility()
{
	SupportedClasses.Add(UAnimSequence::StaticClass());
}

void UStageShiftAnimationSpeedUtility::BakeSpeedData()
{
	const TArray<UObject*> SelectedAnimations = UEditorUtilityLibrary::GetSelectedAssetsOfClass(UAnimSequence::StaticClass());
	if (SelectedAnimations.IsEmpty())
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("BakeSelectedSpeed", "Bake Speed Data"));
	for (UObject* Asset : SelectedAnimations)
	{
		BakeSequence(Cast<UAnimSequence>(Asset));
	}
}

bool UStageShiftAnimationSpeedUtility::BakeSequence(UAnimSequence* Animation)
{
	return BakeCurves(Animation, false);
}

bool UStageShiftAnimationSpeedUtility::BakeFootPlant(UAnimSequence* Animation)
{
	return BakeCurves(Animation, true);
}


#undef LOCTEXT_NAMESPACE
