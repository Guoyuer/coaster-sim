using UnrealBuildTool;

public class CoasterSim : ModuleRules
{
    public CoasterSim(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "Json",
            "Landscape",
            "ProceduralMeshComponent",
            "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Slate",
            "SlateCore"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[]
            {
                "AssetRegistry",
                "MeshConversion",
                "MeshDescription",
                "StaticMeshDescription",
                "UnrealEd"
            });
        }
    }
}
