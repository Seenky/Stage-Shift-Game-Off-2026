// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StageShift : ModuleRules
{
	public StageShift(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", 
			"CoreUObject", 
			"Engine",
			"AIModule",
			"ModelViewViewModel",
			"StatusComponent",
			"AnimGraphRuntime",
			"PhysicsControl",
			"PhysicsCore",
			"InputCore", 
			"EnhancedInput", 
			"GameplayTags", 
			"UMG",
			"SlateCore",
			
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayTags", 
			"AnimationCore",
			"AnimGraphRuntime",
			"OptimusCore",
			"ComputeFramework",
			"ControlRig", 
			"RigVM",
			"PBIK",
			"Slate",
			"NavigationSystem",
			
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
