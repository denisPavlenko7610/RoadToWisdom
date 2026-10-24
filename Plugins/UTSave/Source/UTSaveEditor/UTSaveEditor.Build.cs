// UT Templates - UTSaveEditor module rules.
using UnrealBuildTool;

public class UTSaveEditor : ModuleRules
{
	public UTSaveEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "UTSaveRuntime"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd", "DataValidation", "BlueprintGraph", "UTFoundationRuntime"
		});
	}
}
