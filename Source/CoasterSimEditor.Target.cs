using UnrealBuildTool;
using System.Collections.Generic;

public class CoasterSimEditorTarget : TargetRules
{
    public CoasterSimEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("CoasterSim");
    }
}
