// UT Templates - UTWorldMechanicsRuntime module rules.
using UnrealBuildTool;

public class UTWorldMechanicsRuntime : ModuleRules
{
	public UTWorldMechanicsRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "UTFoundationRuntime", "UTInteractionRuntime", "UTSaveRuntime"
		});

		// Only UTRespawnComponent's implementation touches CombatLite health; no public
		// header includes it, so consumers do not inherit the dependency.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UTCombatLite"
		});
	}
}
