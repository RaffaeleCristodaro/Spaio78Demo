// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "Modules/ModuleManager.h"
#include "SpeechToLifeVoskRecognizer.h"

class FSpeechToLifeVoskModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FSpeechToLifeVoskModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FSpeechToLifeVoskModule>("SpeechToLifeVosk");
	}
	
	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("SpeechToLifeVosk");
	}

	bool IsVoskLoaded() const;

	TSharedPtr<FSpeechToLifeVoskRecognizer, ESPMode::ThreadSafe> VoskRecognizer;

	void vosk_recognizer_free(struct VoskRecognizer *recognizer);
	struct VoskRecognizer *vosk_recognizer_new(struct VoskModel *model, float sample_rate);
	int vosk_recognizer_accept_waveform_f(struct VoskRecognizer *recognizer, const float *data, int length);
	const char *vosk_recognizer_final_result(struct VoskRecognizer *recognizer);
	const char *vosk_recognizer_partial_result(struct VoskRecognizer *recognizer);
	struct VoskModel *vosk_model_new(const char *model_path);
	void vosk_model_free(struct VoskModel *model);
};
