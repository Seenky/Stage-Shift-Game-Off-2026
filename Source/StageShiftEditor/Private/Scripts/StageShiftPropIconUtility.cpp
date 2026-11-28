#include "Scripts/StageShiftPropIconUtility.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PreviewScene.h"
#include "TextureResource.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UnrealClient.h"

#define LOCTEXT_NAMESPACE "StageShiftPropIconUtility"

namespace
{
constexpr int32 IconSize = 256;
constexpr int32 SamplesPerAxis = 3;
constexpr int32 CaptureSize = IconSize * SamplesPerAxis;
const FName SourceMeshKey(TEXT("StageShift.PropIconSource"));

void AddFillLight(FPreviewScene& Scene)
{
	auto* Light = NewObject<UDirectionalLightComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	Light->SetIntensity(1.35f);
	Light->SetLightColor(FLinearColor(0.78f, 0.87f, 1.0f), false);
	Light->SetCastShadows(false);
	Scene.AddComponent(Light, FTransform(FRotator(-28.0, 140.0, 0.0)));
}

bool CapturePixels(USceneCaptureComponent2D& Camera, UTextureRenderTarget2D& Target, ESceneCaptureSource Source,
    TArray<FLinearColor>& Pixels)
{
	Camera.CaptureSource = Source;
	Camera.TextureTarget = &Target;
	Camera.CaptureScene();
	FReadSurfaceDataFlags Flags(RCM_MinMax);
	Flags.SetLinearToGamma(false);
	return Target.GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pixels, Flags)
	    && Pixels.Num() == CaptureSize * CaptureSize;
}

bool RenderIcon(UStaticMesh& Mesh, const FRotator& MeshRotation, TArray<FColor>& Icon)
{
	UObject* SubjectAsset = &Mesh;
	FAssetCompilingManager::Get().FinishCompilationForObjects(MakeArrayView(&SubjectAsset, 1));
	const FBoxSphereBounds Bounds = Mesh.GetBounds();
	if (Bounds.SphereRadius <= UE_KINDA_SMALL_NUMBER) return false;

	FPreviewScene Scene(FPreviewScene::ConstructionValues()
	    .SetCreatePhysicsScene(false).SetTransactional(false)
	    .SetLightRotation(FRotator(-52.0, -107.0, 0.0)).SetLightBrightness(3.1f).SetSkyBrightness(0.75f));
	Scene.SetLightColor(FColor(255, 231, 203));
	if (auto* Cubemap = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap")))
		Scene.SetSkyCubemap(Cubemap);
	Scene.UpdateCaptureContents();
	AddFillLight(Scene);

	auto* Subject = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	Subject->SetMobility(EComponentMobility::Movable);
	Subject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Subject->SetStaticMesh(&Mesh);
	const FQuat Rotation = MeshRotation.Quaternion();
	Scene.AddComponent(Subject, FTransform(Rotation, -Rotation.RotateVector(Bounds.Origin)));

	const FQuat View = FRotator(-36.0, -139.0, 0.0).Quaternion();
	const FVector Right = Rotation.UnrotateVector(View.GetRightVector()).GetAbs();
	const FVector Up = Rotation.UnrotateVector(View.GetUpVector()).GetAbs();
	const double Distance = Bounds.SphereRadius * 4.0 + 50.0;
	auto* Camera = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	Camera->bCaptureEveryFrame = false;
	Camera->bCaptureOnMovement = false;
	Camera->ProjectionType = ECameraProjectionMode::Orthographic;
	Camera->OrthoWidth = 2.42 * FMath::Max(FVector::DotProduct(Bounds.BoxExtent, Right), FVector::DotProduct(Bounds.BoxExtent, Up));
	Camera->ShowFlags.SetFog(false);
	Camera->ShowFlags.SetAtmosphere(false);
	Camera->ShowFlags.SetMotionBlur(false);
	Camera->ShowFlags.SetAntiAliasing(false);
	Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Camera->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Scene.AddComponent(Camera, FTransform(View, -View.GetForwardVector() * Distance));

	auto* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
	Target->ClearColor = FLinearColor::Transparent;
	Target->RenderTargetFormat = RTF_RGBA32f;
	Target->InitAutoFormat(CaptureSize, CaptureSize);
	Target->UpdateResourceImmediate(true);

	TArray<FLinearColor> Color, Depth;
	if (!CapturePixels(*Camera, *Target, SCS_SceneColorHDR, Color)
	    || !CapturePixels(*Camera, *Target, SCS_SceneDepth, Depth)) return false;

	// Average only covered subpixels so the texture keeps its color at transparent edges.
	const double FarDepth = Distance + 2.0 * Bounds.SphereRadius;
	Icon.SetNumUninitialized(IconSize * IconSize);
	bool bHasSubject = false;
	for (int32 Y = 0; Y < IconSize; ++Y)
		for (int32 X = 0; X < IconSize; ++X)
		{
			FLinearColor Sum = FLinearColor::Transparent;
			int32 Covered = 0;
			for (int32 J = 0; J < SamplesPerAxis; ++J)
				for (int32 I = 0; I < SamplesPerAxis; ++I)
				{
					const int32 Index = (Y * SamplesPerAxis + J) * CaptureSize + X * SamplesPerAxis + I;
					if (Depth[Index].R <= 0.0f || Depth[Index].R >= FarDepth) continue;
					Sum += Color[Index];
					++Covered;
				}
			FLinearColor Pixel = Covered ? Sum / Covered : FLinearColor::Transparent;
			Pixel.A = Covered / static_cast<float>(SamplesPerAxis * SamplesPerAxis);
			Icon[Y * IconSize + X] = Pixel.ToFColorSRGB();
			bHasSubject |= Covered > 0;
		}
	return bHasSubject && Icon[0].A == 0;
}
}

UTexture2D* UStageShiftPropIconUtility::RenderBriefingIcon(UStaticMesh* Mesh, FRotator MeshRotation, FText& Error)
{
	Error = FText::GetEmpty();
	if (!IsValid(Mesh))
	{
		Error = LOCTEXT("MissingMesh", "Select a static mesh to render its briefing icon.");
		return nullptr;
	}

	FString MeshName = Mesh->GetName();
	MeshName.RemoveFromStart(TEXT("SM_"));
	const FString TextureName = TEXT("T_Briefing_") + MeshName;
	const FString PackageName = FString::Printf(TEXT("/Game/DEV/UI/StageSignals/BriefingIcons/%s"), *TextureName);
	auto* Existing = LoadObject<UObject>(nullptr, *(PackageName + TEXT(".") + TextureName), nullptr, LOAD_NoWarn);
	auto* Texture = Cast<UTexture2D>(Existing);
	if (Existing && (!Texture || Texture->GetPackage()->GetMetaData().GetValue(Texture, SourceMeshKey) != Mesh->GetPathName()))
	{
		Error = LOCTEXT("NameInUse", "The briefing icon name already belongs to another asset. Rename that icon or the source mesh.");
		return nullptr;
	}

	TArray<FColor> Pixels;
	if (!RenderIcon(*Mesh, MeshRotation, Pixels))
	{
		Error = LOCTEXT("EmptyCapture", "Could not render the mesh with a transparent background.");
		return nullptr;
	}

	if (Texture) Texture->Modify();
	else Texture = NewObject<UTexture2D>(CreatePackage(*PackageName), *TextureName, RF_Public | RF_Standalone | RF_Transactional);
	Texture->GetPackage()->GetMetaData().SetValue(Texture, SourceMeshKey, *Mesh->GetPathName());
	Texture->Source.Init(IconSize, IconSize, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
	Texture->CompressionSettings = TC_EditorIcon;
	Texture->LODGroup = TEXTUREGROUP_UI;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->SRGB = true;
	Texture->PostEditChange();
	Texture->MarkPackageDirty();
	if (!Existing) FAssetRegistryModule::AssetCreated(Texture);
	return Texture;
}

#undef LOCTEXT_NAMESPACE
