// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RoadToWisdom : ModuleRules
{
	public RoadToWisdom(ReadOnlyTargetRules Target) : base(Target)
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
			"SlateCore"
		});

		// UnrealTemplates domain modules this project calls directly (see
		// Docs/ToolkitIntegrationMatrix.md). UTCombat is present as UTWorldMechanics'
		// dependency but is intentionally not declared: the game calls no combat API yet.
		PublicDependencyModuleNames.AddRange(new string[] {
			"UTFoundationRuntime",
			"UTInteractionRuntime",
			"UTSaveRuntime",
			"UTItemsRuntime",
			"UTInventoryRuntime",
			"UTWorldMechanicsRuntime"
		});

		PublicIncludePaths.AddRange(new string[] {
			"RoadToWisdom",
			"RoadToWisdom/SportsCar",
			"RoadToWisdom/OffroadCar",
			"RoadToWisdom/Variant_OffRoad",
			"RoadToWisdom/Variant_TimeTrial",
			"RoadToWisdom/Variant_TimeTrial/UI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
		if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("UnrealEd");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
