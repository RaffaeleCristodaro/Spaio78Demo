// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "CoreMinimal.h"
#include "SpeechToLifeRecognizerInterface.h"
#include "Modules/ModuleManager.h"

SPEECHTOLIFE_API DECLARE_LOG_CATEGORY_EXTERN(LogSpeechToLife, Log, All);

DECLARE_STATS_GROUP(TEXT("SpeechToLife"), STATGROUP_SpeechToLife, STATCAT_Advanced);

/**
 * Speech to Life Module
 */
class FSpeechToLifeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	ISpeechToLifeRecognizerPtr GetRecognizer(const FName& recognizerName)
	{
		if (Recognizers.Contains(recognizerName))
		{
			return Recognizers[recognizerName];
		}

		// Try to load the module
		const FName recognizerModuleName = *FString::Printf(TEXT("SpeechToLife%s"), *recognizerName.ToString());
		FModuleManager& ModuleManager = FModuleManager::Get();
		if (!ModuleManager.IsModuleLoaded(recognizerModuleName))
		{
			// Attempt to load the module
			ModuleManager.LoadModule(recognizerModuleName);
		}

		if (Recognizers.Contains(recognizerName))
		{
			return Recognizers[recognizerName];
		}

		UE_LOG(LogSpeechToLife, Error, TEXT("GetRecognizer: Unable to find/load recognizer named '%s'!"), *recognizerName.ToString());
		return nullptr;
	}

	bool RegisterRecognizer(const FName& recognizerName, ISpeechToLifeRecognizerPtr recognizer)
	{
		if (!Recognizers.Contains(recognizerName))
		{
			Recognizers.Add(recognizerName, recognizer);
			return true;
		}

		return false;
	}

	bool UnregisterRecognizer(const FName& recognizerName)
	{
		if (Recognizers.Contains(recognizerName))
		{
			Recognizers.Remove(recognizerName);
			return true;
		}

		return false;
	}

private:

	TMap<FName, ISpeechToLifeRecognizerPtr> Recognizers;
};
