// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class T5GInventorySystem : ModuleRules
{
	public T5GInventorySystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				//"T5GInventorySystem/Public","T5GInventorySystem/Public/lib",
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				//"T5GInventorySystem/Private","T5GInventorySystem/Private/lib"
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"EnhancedInput"
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
