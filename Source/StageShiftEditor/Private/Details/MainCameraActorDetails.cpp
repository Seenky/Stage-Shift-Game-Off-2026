#include "Details/MainCameraActorDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FMainCameraActorDetails::MakeInstance()
{
	return MakeShared<FMainCameraActorDetails>();
}

void FMainCameraActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.SortCategories([](const TMap<FName, IDetailCategoryBuilder*>& Categories)
	{
		static const TSet<FName> VisibleCategories = {
		    TEXT("CameraSettings"), TEXT("CameraOptions"), TEXT("Camera"), TEXT("CameraCollision"), TEXT("Lag"), TEXT("PostProcess"), TEXT("Transform"), TEXT("TransformCommon")};

		for (const auto& Category : Categories)
		{
			Category.Value->SetCategoryVisibility(VisibleCategories.Contains(Category.Key));
		}
	});
}
