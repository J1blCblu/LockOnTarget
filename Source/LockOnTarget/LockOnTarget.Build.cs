// Copyright 2022-2023 Ivan Baktenkov. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LockOnTarget : ModuleRules
{
	public LockOnTarget(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		//Non-Unity build.

		//https://forums.unrealengine.com/t/how-to-compile-in-non-unity-mode/94863/5
		//Add the following to *project*Editor.Target.cs to activate for the project.
		//bUseUnityBuild = false;
		//bUsePCHFiles = false;
		
		//Or uncomment this for the current module.
		//bEnforceIWYU = true;
		//bUseUnity = false;

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "Engine",

            }
			);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				//"Slate",
				//"SlateCore",
				"UMG",
				"Projects",
				"NetCore",
				"TraceLog",

            }
			);
	}
}
