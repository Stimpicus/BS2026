// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BS2026 : ModuleRules
{
	public BS2026(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"UMG",
			"Slate",
			// Networking
			"NetCore",
			"NetworkPrediction",
			// Gameplay Ability System
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			// Online
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});

		PublicIncludePaths.AddRange(new string[] {
			"BS2026",
			"BS2026/SportsCar",
			"BS2026/OffroadCar",
			"BS2026/Variant_Offroad",
			"BS2026/Variant_TimeTrial",
			"BS2026/Variant_TimeTrial/UI",
			// Multiplayer subsystems
			"BS2026/Networking",
			"BS2026/Abilities",
			"BS2026/Session",
			"BS2026/Experience"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}
