using UnrealBuildTool;
using System.Collections.Generic;

public class CoasterSimTarget : TargetRules
{
    public CoasterSimTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("CoasterSim");
    }
}
