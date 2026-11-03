using UnrealBuildTool;

public class AlphaGame : ModuleRules
{
    public AlphaGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "UMG", "Slate", "SlateCore"
        });
        PublicIncludePaths.Add(ModuleDirectory);
    }
}
