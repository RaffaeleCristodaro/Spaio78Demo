// Copyright 2020-2023 Solar Storm Interactive

#include "SpeechToLifeSession.h"
#include "HAL/RunnableThread.h"

int32 FSpeechToLifeSession::SessionIncrementingID = 0;

// Declare the static loading complete delegate
FSpeechToLifeSession::FModelLoadingComplete FSpeechToLifeSession::ModelLoadingComplete; 

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeSession::StartSession()
{
	if(!IsRunning)
	{
		IsRunning = true;
		SessionThread = FRunnableThread::Create(this, *FString::Printf(TEXT("SpeechToLifeSession_%u"), SessionIncrementingID++));
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeSession::TerminateSession()
{
	IsRunning = false;
	if(SessionThread)
	{
		SessionThread->WaitForCompletion();
		SessionThread = nullptr;
	}
	if(InputQueue)
	{
		InputQueue->Empty();
	}
	if(ResultQueue)
	{
		ResultQueue->Empty();
	}
	ShouldBeListening = false;
	SessionParametersChanged = false;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeSession::PushAudio(const FSessionAudioBuffer& audioIn) const
{
	if(InputQueue.IsValid() && ShouldBeListening)
	{
		InputQueue->Enqueue(audioIn);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeSession::FetchResults(TArray<FSTLSpeechResult>& resultsOut) const
{
	if(ResultQueue.IsValid() && !ResultQueue->IsEmpty())
	{
		FSessionRecognitionResult result;
		while(ResultQueue->Dequeue(result))
		{
			resultsOut.Append(result.Results);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
uint32 FSpeechToLifeSession::Run()
{
	while(IsRunning && RunRecognition())
	{
	}

	CleanupRecognition();
	return 0;
}
