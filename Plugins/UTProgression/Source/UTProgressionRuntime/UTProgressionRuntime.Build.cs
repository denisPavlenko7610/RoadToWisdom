// UT Templates - UTProgressionRuntime module rules.
using UnrealBuildTool;

public class UTProgressionRuntime : ModuleRules
{
	public UTProgressionRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "UTFoundationRuntime"
		});
	}
}
