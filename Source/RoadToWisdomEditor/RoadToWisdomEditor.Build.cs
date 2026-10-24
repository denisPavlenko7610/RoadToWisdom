using UnrealBuildTool;
public class RoadToWisdomEditor : ModuleRules
{
    public RoadToWisdomEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine"});
        PrivateDependencyModuleNames.AddRange(new[]{"UnrealEd","Landscape"});
    }
}
