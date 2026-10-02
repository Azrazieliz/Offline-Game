using UnrealBuildTool;
using System.Collections.Generic;

public class OfflineGameTarget : TargetRules
{
    public OfflineGameTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("OfflineGame");
    }
}
