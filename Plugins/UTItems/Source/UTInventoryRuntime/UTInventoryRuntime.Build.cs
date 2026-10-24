// UT Templates - UTInventoryRuntime module rules.
using UnrealBuildTool;

public class UTInventoryRuntime : ModuleRules
{
	public UTInventoryRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "UTItemsRuntime", "UTInteractionRuntime", "UTSaveRuntime", "UTFoundationRuntime", "NetCore"
		});
	}
}
