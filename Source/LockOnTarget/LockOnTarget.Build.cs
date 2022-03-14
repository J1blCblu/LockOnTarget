// Copyright 2022 Ivan Baktenkov. All Rights Reserved.

using UnrealBuildTool;

public class LockOnTarget : ModuleRules
{
	public LockOnTarget(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		//https://forums.unrealengine.com/t/how-to-compile-in-non-unity-mode/94863/5
		//bUseUnityBuild = false;
		//bUsePCHFiles = false;

		//Lock On Target Unreal Insights preprocessor definition.
		PublicDefinitions.Add("LOC_INSIGHTS = 0");

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UMG",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
