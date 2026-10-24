// UT Templates - UTInteractionRuntime module rules.
using UnrealBuildTool;

public class UTInteractionRuntime : ModuleRules
{
	public UTInteractionRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine"
		});
	}
}
