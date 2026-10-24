// UT Templates - UTFoundationRuntime module rules.
using UnrealBuildTool;

public class UTFoundationRuntime : ModuleRules
{
	public UTFoundationRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags"
		});
	}
}
