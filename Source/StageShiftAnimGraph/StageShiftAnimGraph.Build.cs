using UnrealBuildTool;

public class StageShiftAnimGraph : ModuleRules
{
	public StageShiftAnimGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AnimGraph", "AnimGraphRuntime", "StageShift" });
		PrivateDependencyModuleNames.Add("BlueprintGraph");
	}
}
