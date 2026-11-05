// Copyright 2026 CodeLikeMe. All Rights Reserved.

using UnrealBuildTool;

public class AssetproLobbyKit : ModuleRules
{
	public AssetproLobbyKit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG",
			"CommonUI",
			"CommonInput",
			"EnhancedInput",
			"GameplayTags",
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MoviePlayer",
			"ApplicationCore",
			"EngineSettings",
			"ImageCore",
			// Online: the engine's session interface (no platform SDK), and the socket layer for lobby codes.
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"Sockets",
			"NetCore",
			"CoreOnline"
		});
	}
}
