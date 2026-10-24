// UT Templates - UTProgressionEditor module rules.
using UnrealBuildTool;

public class UTProgressionEditor : ModuleRules
{
	public UTProgressionEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "UTProgressionRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd", "DataValidation", "AssetRegistry"
		});
	}
}
