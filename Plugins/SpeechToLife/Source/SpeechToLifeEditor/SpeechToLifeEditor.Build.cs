// Copyright 2020-2023 Solar Storm Interactive

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SpeechToLifeEditor : ModuleRules
	{
		public SpeechToLifeEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					//"Engine",
					//"UnrealEd",
					"SpeechToLife",
				}
			);
			
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Settings",
				}
			);
		}
	}
}
