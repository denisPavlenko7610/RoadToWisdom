// UT Templates - UTCombatLite module rules.
using UnrealBuildTool;

public class UTCombatLite : ModuleRules
{
	public UTCombatLite(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "UTFoundationRuntime", "UTCombatCore"
		});
	}
}
