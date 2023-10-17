// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "SpeechToLifeRecognizerInterface.h"
#include "SpeechToLifeVoskSession.h"

/**
 * All recognizers implement this interface to interact with them
 */
class FSpeechToLifeVoskRecognizer : public ISpeechToLifeRecognizerInterface
{
public:
	virtual FName GetRecognizerName() const override
	{
		return FName(TEXT("Vosk"));
	}

	virtual TUniquePtr<FSpeechToLifeSession> CreateSession(const FName& localID, const FString& pathToModel) override
	{
		auto voskSession = MakeUnique<FSpeechToLifeVoskSession>(GetRecognizerName());
		if(!localID.IsNone() && !pathToModel.IsEmpty())
		{
			voskSession->SetModel(localID, pathToModel);
		}
		
		return voskSession;
	}
};
