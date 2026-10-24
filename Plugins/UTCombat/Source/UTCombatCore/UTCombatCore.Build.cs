// UT Templates - UTCombatCore module rules.
using UnrealBuildTool;

public class UTCombatCore : ModuleRules
{
	public UTCombatCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "UTFoundationRuntime"
		});
	}
}
