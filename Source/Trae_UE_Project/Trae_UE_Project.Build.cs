// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Trae_UE_Project : ModuleRules
{
	public Trae_UE_Project(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"Slate", "SlateCore", "UMG", "ApplicationCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// 程序化降雨器 UProceduralMeshComponent（引擎自带 Runtime 插件模块）
			"ProceduralMeshComponent"
		});
	}
}
