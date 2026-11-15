using UnrealBuildTool;

public class StageShiftEditor : ModuleRules
{
	public StageShiftEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Blutility", "StageShift" });
		PrivateDependencyModuleNames.AddRange(new[] { "AnimationBlueprintLibrary", "UnrealEd", "StageShift", "PropertyEditor", "GraphEditor", "Slate", "SlateCore",
			"AssetDefinition", "AssetTools", "ToolMenus", "InputCore", "GameplayTags", "AssetRegistry", "ApplicationCore", "StructUtilsEditor" });
	}
}
