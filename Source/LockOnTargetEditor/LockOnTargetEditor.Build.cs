// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LockOnTargetEditor : ModuleRules
{
	public LockOnTargetEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				//"LockOnTargetEditor/Public"
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				//"LockOnTargetEditor/Private"
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"LockOnTarget"
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
				"EditorStyle",
				"UnrealEd",

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
