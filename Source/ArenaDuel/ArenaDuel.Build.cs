// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ArenaDuel : ModuleRules
{
	public ArenaDuel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayAbilities", "GameplayTags", "GameplayTasks", "UMG", "RHI", "AnimationCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "MeshDescription", "StaticMeshDescription", "AIModule", "NavigationSystem", "Sockets" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "CQTest", "EngineSettings", "IrisCore", "LevelEditor", "UnrealEd", "MeshDescription", "SkeletalMeshDescription" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
