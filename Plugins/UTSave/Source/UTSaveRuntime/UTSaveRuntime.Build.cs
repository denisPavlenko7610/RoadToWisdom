// UT Templates - UTSaveRuntime module rules.
using UnrealBuildTool;

public class UTSaveRuntime : ModuleRules
{
	public UTSaveRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "UTFoundationRuntime"
		});
	}
}
