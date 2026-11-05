// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThirdPersonTemplate : ModuleRules
{
	public ThirdPersonTemplate(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"ThirdPersonTemplate",
			"ThirdPersonTemplate/Variant_Platforming",
			"ThirdPersonTemplate/Variant_Platforming/Animation",
			"ThirdPersonTemplate/Variant_Combat",
			"ThirdPersonTemplate/Variant_Combat/AI",
			"ThirdPersonTemplate/Variant_Combat/Animation",
			"ThirdPersonTemplate/Variant_Combat/Gameplay",
			"ThirdPersonTemplate/Variant_Combat/Interfaces",
			"ThirdPersonTemplate/Variant_Combat/UI",
			"ThirdPersonTemplate/Variant_SideScrolling",
			"ThirdPersonTemplate/Variant_SideScrolling/AI",
			"ThirdPersonTemplate/Variant_SideScrolling/Gameplay",
			"ThirdPersonTemplate/Variant_SideScrolling/Interfaces",
			"ThirdPersonTemplate/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
