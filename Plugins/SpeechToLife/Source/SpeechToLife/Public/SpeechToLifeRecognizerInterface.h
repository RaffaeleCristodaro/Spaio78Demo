// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "CoreMinimal.h"
#include "SpeechToLifeSession.h"

/**
 * All recognizers implement this interface to interact with them
 */
class ISpeechToLifeRecognizerInterface
{
public:
	virtual ~ISpeechToLifeRecognizerInterface() {}

	/**
	 * @return Return the name of the recognizer
	 */
	virtual FName GetRecognizerName() const = 0;

	/**
	 * @brief Create a new recognizer session
	 * @param localID [optional] The ID of the locale related to the path. Can be set later via session SetModel().
	 * @param pathToModel [optional] The filesystem path to the model data folder. Can be set later session SetModel().
	 *					  Might not be important for all recognizers.
	 * @return A pointer to the session
	 */
	virtual TUniquePtr<FSpeechToLifeSession> CreateSession(const FName& localID, const FString& pathToModel) = 0;
};

typedef TSharedPtr<ISpeechToLifeRecognizerInterface, ESPMode::ThreadSafe> ISpeechToLifeRecognizerPtr;
