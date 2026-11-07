using UnrealBuildTool;

public class StageShiftEditor : ModuleRules
{
	public StageShiftEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Blutility" });
		PrivateDependencyModuleNames.AddRange(new[]
		{
			"AnimationBlueprintLibrary", "UnrealEd"
		});
	}
}
