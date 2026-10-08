using UnrealBuildTool;
using System.IO;

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
            "GameplayTags",
            "ProceduralMeshComponent"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AndroidPermission",
            "ApplicationCore",
            "InputCore",
            "Json",
            "JsonUtilities",
            "PakFile",
            "PlatformCrypto",
            "PlatformCryptoContext",
            "PlatformCryptoTypes",
            "SQLiteCore",
            "Slate",
            "SlateCore",
            "UMG"
        });

        RuntimeDependencies.Add(
            "$(ProjectDir)/Content/Data/FoundationClockPolicy.json",
            StagedFileType.UFS);

        RuntimeDependencies.Add(
            "$(ProjectDir)/Config/Foundation/CharacterVisualDiagnostic.json",
            StagedFileType.UFS);

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            AdditionalPropertiesForReceipt.Add(
                "AndroidPlugin",
                Path.Combine(ModuleDirectory, "OfflineGame_APL.xml"));
        }
    }
}