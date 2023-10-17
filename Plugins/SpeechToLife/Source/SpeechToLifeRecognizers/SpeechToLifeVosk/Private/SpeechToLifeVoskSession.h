// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "SpeechToLifeSession.h"

class FSpeechToLifeVoskSession : public FSpeechToLifeSession
{
public:

	FSpeechToLifeVoskSession(const FName& recognizerID)
		: FSpeechToLifeSession(recognizerID)
		, IsListening(false)
		, VoskRecognizerPtr(nullptr)
		, VoskModelPtr(nullptr)
	{
		
	}

	virtual bool RunRecognition() override;
	virtual void CleanupRecognition() override;

private:

	// Two scratch buffers for conversions
	TArray<float> scratch1;
	TArray<float> scratch2;

	// True if we are listening. Syncs with ShouldBeListening.
	bool IsListening;

	// The initialized recognizer
	struct VoskRecognizer* VoskRecognizerPtr;

	// The referenced vosk model
	struct VoskModel* VoskModelPtr;

	// What we are expecting the current model to be set to
	FName CurrentLocalID;

	// ***************************************************************************************************>
	// Static model loading below. All sessions share models and lock resources using VoskModelsMutex
	struct FVoskModelHolder
	{
		struct VoskModel* VoskModelPtr = nullptr;
		int32 RefCount = 0;
	};

	static FCriticalSection VoskModelsMutex;

	// Current globaly loaded vosk models, read / write locked via VoskModelsMutex
	static TMap<FName, FVoskModelHolder> LoadedVoskModels;

	static VoskModel* ReferenceVoskModel(const FName& LocalID, const FString& pathToModel);
	static void DereferenceVoskModel(const FName& LocalID);
};
