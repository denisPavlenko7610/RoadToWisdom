// UT Templates - UTItemsEditor module rules.
using UnrealBuildTool;

public class UTItemsEditor : ModuleRules
{
	public UTItemsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "UTItemsRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd", "DataValidation", "AssetRegistry"
		});
	}
}
