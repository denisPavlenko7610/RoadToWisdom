// UT Templates - UTEquipmentRuntime module rules.
using UnrealBuildTool;

public class UTEquipmentRuntime : ModuleRules
{
	public UTEquipmentRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "GameplayTags", "UTFoundationRuntime", "UTItemsRuntime"
		});
	}
}
