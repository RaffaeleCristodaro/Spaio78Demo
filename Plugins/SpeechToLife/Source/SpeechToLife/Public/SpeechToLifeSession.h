// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"
#include "SpeechToLifeResult.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
#include "HAL/ThreadSafeBool.h"
#endif

/**
 * @brief An audio buffer to send to the session
 */
 struct FSessionAudioBuffer
{
 	// The frames of audio
	TArray<float> Frames;
 	
 	// The sample rate of the audio
	uint32 SampleRate;
 	
 	// How many channels
	uint32 Channels;
 	
 	// Are the channels interleaved
	bool Interleaved;
};

struct FSessionRecognitionResult
{
	TArray<FSTLSpeechResult> Results;
};

/**
 * Speech Recoqnition session runnable. Push audio in via PushAudio, and periodically FetchResults
 */
class SPEECHTOLIFE_API FSpeechToLifeSession : public FRunnable
{
public:
	using FSessionAudioInputQueue = TQueue<FSessionAudioBuffer, EQueueMode::Spsc>;
	using FSessionResultQueue = TQueue<FSessionRecognitionResult, EQueueMode::Spsc>;

	FSpeechToLifeSession(const FName& recognizerID)
		: SessionThread(nullptr)
		, InputQueue(MakeUnique<FSessionAudioInputQueue>())
		, ResultQueue(MakeUnique<FSessionResultQueue>())
		, RecognizerID(recognizerID)
		, SessionParametersChanged(false)
	{
		IsRunning = false;
		ShouldBeListening = false;
	}
	virtual ~FSpeechToLifeSession() override
	{
		IsRunning = false;
		checkf(SessionThread == nullptr, TEXT("Session should have been torn down via TerminateSession() by now"));
		InputQueue = nullptr;
		ResultQueue = nullptr;
		SessionThread = nullptr;
		ShouldBeListening = false;
		SessionParametersChanged = false;
	}
	
	// Helper function to get the recognizer ID of this session
	FORCEINLINE FName GetRecognizerID() const { return RecognizerID; }

	/**
	 * Set the model to use
	 */
	void SetModel(const FName& localID, const FString& pathToModel)
	{
		FScopeLock Lock(&SessionModificationMutex);
		LocalID = localID;
		PathToModel = pathToModel;
		SessionParametersChanged = true;
	}

	/**
	 * Clear out the current model, leaving the recognizer in an uninitialized state
	 */
	void ClearModel()
	{
		FScopeLock Lock(&SessionModificationMutex);
		LocalID = NAME_None;
		PathToModel = TEXT("");
		SessionParametersChanged = true;
	}

	/**
	 * @brief Start listening, and start the helper recognizer thread if needed
	 */ 
	void StartListening(bool reset = false)
	{
		checkf(IsRunning, TEXT("StartSession should be called before calling StartListening!"));
		
		if(reset)
		{
			if(ResultQueue)
			{
				ResultQueue->Empty();
			}
			SessionParametersChanged = true;
		}
		ShouldBeListening = true;
	}

	/**
	 * @brief Stops the session from listening to new audio data, but does not terminate the running thread. Use TerminateSession() if you want to clear this session completely.
	 */
	void StopListening()
	{
		ShouldBeListening = false;
	}

	/**
	 * @brief Starts the session. This creates the runnable thread and as such begins the processing of audio samples for recognition.
	 *        Must call StartListening before recognition will be run, otherwise audio samples are discarded.
	 */
	void StartSession();

	/**
	 * @brief Terminates the current session and thread leaving this object in a (ready to be used again) state.
	 */
	void TerminateSession();

	/**
	 * @return Returns true if should be listening, as in, StartListening was called.
	 */
	 bool GetShouldBeListening() const
	{
		return ShouldBeListening;
	}

	/**
	 * @brief Push the audio buffer to be recognized
	 * @param audioIn 
	 */
	void PushAudio(const FSessionAudioBuffer& audioIn) const;

	/**
	 * @brief Call to fetch the latest results from the recognizer
	 */
	void FetchResults(TArray<FSTLSpeechResult>& resultsOut) const;

	virtual uint32 Run() override final;
	virtual void Stop() override final
	{
		IsRunning = false;
	}

	/**
	 * @brief Implement to run recognition
	 * @return Return if recognition should continue
	 */
	 virtual bool RunRecognition() = 0;
	/**
	 * Cleanup anything about recognition before thread termination
	 */
	virtual void CleanupRecognition() = 0;

	/**
	 * Called when a model has finished loading on the session thread
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FModelLoadingComplete, FName);
	static FModelLoadingComplete ModelLoadingComplete;

private:

	// The created thread
	class FRunnableThread* SessionThread;

	// Just a unique ID assigned and incremented per session thread
	static int32 SessionIncrementingID;

protected:

	// The input queue the session will pull from
	TUniquePtr<FSessionAudioInputQueue> InputQueue;

	// Results generated from the recognizer
	TUniquePtr<FSessionResultQueue> ResultQueue;

	// True if the session should be running
	FThreadSafeBool IsRunning;

	// True if the receiver should be listening to incomming audio
	FThreadSafeBool ShouldBeListening;

	// The ID of the model locale being used
	FName LocalID;

	// Get the recognizer ID
	FName RecognizerID;

	// The filesystem path to the model folder
	FString PathToModel;

	// Used to lock when modifying the sessions parameters at runtime
	FCriticalSection SessionModificationMutex;

	// True when the sessions parameters have changed and the sessions needs to be re-initialized
	FThreadSafeBool SessionParametersChanged;
};
