// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class LockOnTarget : ModuleRules
{
	public LockOnTarget(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		//https://forums.unrealengine.com/t/how-to-compile-in-non-unity-mode/94863/5
		//Add in *project*Editor.Target.cs
		//bUseUnityBuild = false;
		//bUsePCHFiles = false;

		//Lock On Target Unreal Insights preprocessor definition.
		PublicDefinitions.Add("LOT_INSIGHTS = 0");

		PublicIncludePaths.AddRange(
			new string[] 
			{
				Path.Combine(ModuleDirectory, "Public/Utilities"),
			}
			);
				
		PrivateIncludePaths.AddRange(
			new string[] 
			{

			}
			);
			
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
			}
			);

        DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{

			}
			);
	}
}
