// UT Templates - UTWorldMechanicsRuntime module rules.
using UnrealBuildTool;

public class UTWorldMechanicsRuntime : ModuleRules
{
	public UTWorldMechanicsRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "UTFoundationRuntime", "UTInteractionRuntime", "UTCombatLite", "UTSaveRuntime"
		});
	}
}
