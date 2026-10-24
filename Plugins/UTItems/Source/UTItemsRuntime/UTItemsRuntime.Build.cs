// UT Templates - UTItemsRuntime module rules.
using UnrealBuildTool;

public class UTItemsRuntime : ModuleRules
{
	public UTItemsRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags"
		});
	}
}
