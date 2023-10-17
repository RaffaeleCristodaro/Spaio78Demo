// Copyright 2020-2023 Solar Storm Interactive

#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "SpeechToLifeSubsystem.h"

#define LOCTEXT_NAMESPACE "SpeechToLifeEditorLoc"

//--------------------------------------------------------------------------------------------------------------------
/**
*/
class FSpeechToLifeEditorModule : public IModuleInterface
{
public:
	
	virtual void StartupModule() override
	{
		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Speech To Life",
				LOCTEXT("SpeechToLifeSettingsName", "Speech To Life"),
				LOCTEXT("SpeechToLifeSettingsDescription", "Configure the Speech To Life plug-in."),
				GetMutableDefault<USpeechToLifeSubsystem>()
			);
		}
	}
	virtual void ShutdownModule() override
	{
		
	}
};

IMPLEMENT_GAME_MODULE(FSpeechToLifeEditorModule, SpeechToLifeEditor);

#undef LOCTEXT_NAMESPACE
