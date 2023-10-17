// Copyright 2020-2023 Solar Storm Interactive

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SpeechToLife : ModuleRules
	{
		public SpeechToLife(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			//OptimizeCode = CodeOptimization.Never;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"AudioMixer",
					"SignalProcessing",
					"AudioCapture",
					"AudioCaptureCore",
				}
			);

			PrivateDependencyModuleNames.AddRange(new string[] { "Projects", "RapidFuzz" });
			
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PrivateDependencyModuleNames.Add("Launch");
				
				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "SpeechToLife_APL.xml"));
			}
		}
	}
}
