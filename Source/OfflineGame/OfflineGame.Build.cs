using UnrealBuildTool;

public class OfflineGame : ModuleRules
{
    public OfflineGame(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "GameplayTags"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "ApplicationCore",
            "InputCore",
            "Json",
            "JsonUtilities",
            "SQLiteCore",
            "Slate",
            "SlateCore",
            "UMG"
        });
    }
}
