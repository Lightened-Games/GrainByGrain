// Pocket Sand - game module.

using UnrealBuildTool;

public class PocketSand : ModuleRules
{
	public PocketSand(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Slate", "SlateCore", "ProceduralMeshComponent", "EngineCameras",
			"Niagara", "Json", "AnimGraphRuntime", "RenderCore", "RHI",
			// FactoryKit (start screen, cinematics, take-over autoplay, legend) and FactoryRecord (game-audio capture).
			"DeveloperSettings", "AudioMixer"
		});

		// Editor-only helpers used by the content script (BlockingVolume brushes, Niagara renderer materials).
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.Add(ModuleDirectory);
	}
}