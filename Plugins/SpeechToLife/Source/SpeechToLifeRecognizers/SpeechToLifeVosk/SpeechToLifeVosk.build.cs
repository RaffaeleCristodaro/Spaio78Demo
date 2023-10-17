// Copyright 2020-2023 Solar Storm Interactive

using UnrealBuildTool;
using System.IO;

public class SpeechToLifeVosk : ModuleRules
{
	private string ThirdPartyPath
	{
       	get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../Source/ThirdParty")); }
	}
	
	private string LibrariesPath
	{
		get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../Libraries", Target.Platform.ToString())); }
	}

	private string VoskPath
	{
		get { return Path.GetFullPath(Path.Combine(ThirdPartyPath, "vosk")); }
	}

	private string VoskIncludePath
	{
		get { return Path.GetFullPath(Path.Combine(VoskPath, "include")); }
	}

	private string VoskLibPath
	{
		get { return Path.GetFullPath(Path.Combine(VoskPath, "bin", Target.Platform.ToString())); }
	}

	public SpeechToLifeVosk(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		//OptimizeCode = CodeOptimization.Never;
		
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", 
			"CoreUObject", 
			"Engine", 
			"SpeechToLife", 
			"Json", 
			"SignalProcessing",
		});
		PublicIncludePaths.Add(VoskIncludePath);

		string[] VoskBins = new string[] {};
		string[] VoskLibs = new string[] {};

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			VoskBins = new string[]
			{
				"libvosk.dylib"
			};
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDependencyModuleNames.Add("Projects");
			
			VoskBins = new string[]
			{
				"libgcc_s_seh-1.dll",
				"libstdc++-6.dll",
				"libvosk.dll",
				"libwinpthread-1.dll"
			};
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			VoskLibs = new string[]
			{
				"arm64-v8a/libvosk.so",
				"armeabi-v7a/libvosk.so",
				"x86/libvosk.so",
				"x86_64/libvosk.so"
			};
			
			// Extra instructions for including the correct lib and loading it
			// Also for including the model data in the assets directory
			string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "SpeechToLifeVosk_APL.xml"));
		}

		if (!Directory.Exists(LibrariesPath))
		{
			Directory.CreateDirectory(LibrariesPath);
		}
		
		foreach (string VoskBin in VoskBins)
		{
			PublicDelayLoadDLLs.Add(Path.Combine(LibrariesPath, VoskBin));
			if (!File.Exists(Path.Combine(LibrariesPath, VoskBin)))
			{
				File.Copy(Path.Combine(VoskLibPath, VoskBin), Path.Combine(LibrariesPath, VoskBin));
			}
			RuntimeDependencies.Add(Path.Combine(LibrariesPath, VoskBin));
		}
		
		foreach (string VoskLib in VoskLibs)
		{
			PublicAdditionalLibraries.Add(Path.Combine(VoskLibPath, VoskLib));
		}
	}
} 
