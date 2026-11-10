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
			"AnimGraphRuntime",
			"InputCore", 
			"EnhancedInput", 
			"GameplayTags", 
			"UMG",
			"SlateCore",
			
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AIModule", 
			"StateTreeModule", 
			"GameplayStateTreeModule", 
			"GameplayTags", 
			"AnimationCore",
			"OptimusCore",
			"ComputeFramework",
			"ControlRig", 
			"RigVM",
			"PBIK",
			"Slate",
			"ModelViewViewModel"
			
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
