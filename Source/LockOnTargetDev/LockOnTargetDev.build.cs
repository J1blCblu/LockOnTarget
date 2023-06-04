// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

using UnrealBuildTool;

public class LockOnTargetDev : ModuleRules
{
    public LockOnTargetDev(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "LockOnTarget",
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",

            }
            );

        SetupGameplayDebuggerSupport(Target);
    }
}
