// Copyright 2020-2023 Solar Storm Interactive

#include "SpeechToLifeVoskSession.h"
#include "SpeechToLifeFunctionLibrary.h"
#include "SpeechToLifeModule.h"
#include "Serialization/JsonSerializer.h"
#include "SpeechToLifeVoskModule.h"
#include "Async/Async.h"

#define VOSK_INPUT_SAMPLE_RATE 16000

DECLARE_LOG_CATEGORY_EXTERN(LogSpeechToLifeVosk, Log, All);
DEFINE_LOG_CATEGORY(LogSpeechToLifeVosk);

DECLARE_CYCLE_STAT(TEXT("Vosk"), StL_VoskRecognition, STATGROUP_SpeechToLife);

namespace VoskSession
{
	//---------------------------------------------------------------------------------------------------------------------
	/**
	*/
	static bool GetResult(TSharedPtr<FJsonObject> jsonObj, FSTLSpeechResult& result)
	{
		result.Clear();

		// Check for the overall confidence field
		double ResultConfidence;
		if (jsonObj->TryGetNumberField(TEXT("confidence"), ResultConfidence))
		{
			result.Confidence = ResultConfidence;
		}
	
		if (!jsonObj->TryGetStringField(TEXT("text"), result.Sentence))
		{
			if (jsonObj->TryGetStringField(TEXT("partial"), result.Sentence))
			{
				result.Type = ESTLResultType::Partial;
			}
			else
			{
				UE_LOG(LogSpeechToLife, Error, TEXT("Invalid Object"));
				return false;
			}
		}
		else
		{
			result.Type = ESTLResultType::Final;
		}

		if(result.Sentence.Len() == 0)
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Words;
		if (jsonObj->TryGetArrayField(TEXT("result"), Words))
		{
			for (const TSharedPtr<FJsonValue>& WordJsonValue : *Words)
			{
				const TSharedPtr<FJsonObject>& WordObject = WordJsonValue->AsObject();
				FString word;
				double confidence = -1.0;
				double startTime = -1.0;
				double endTime = -1.0;

				WordObject->TryGetNumberField(TEXT("conf"), confidence);

				const bool Success = WordObject->TryGetStringField(TEXT("word"), word) &&
									 WordObject->TryGetNumberField(TEXT("start"), startTime) &&
									 WordObject->TryGetNumberField(TEXT("end"), endTime);
				if (Success)
				{
					result.AddWord(word, confidence, startTime, endTime);
				}
				else
				{
					UE_LOG(LogSpeechToLife, Error, TEXT("Invalid Word"));
				}
			}
		}
		else
		{
			result.SplitSentence();
		}

		return true;
	}

	//---------------------------------------------------------------------------------------------------------------------
	/**
	*/
	static bool ParseJson(const FString& jsonString, TArray<FSTLSpeechResult>& resultsOut)
	{
		TSharedPtr<FJsonObject> jsonObject = MakeShareable(new FJsonObject());
		const TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(jsonString);

		if (FJsonSerializer::Deserialize(jsonReader, jsonObject))
		{
			FSTLSpeechResult tmpResult;
			const TArray<TSharedPtr<FJsonValue>>* Alternatives;
			if (jsonObject->TryGetArrayField(TEXT("alternatives"), Alternatives))
			{
				for (const TSharedPtr<FJsonValue>& AlternativeJsonValue : *Alternatives)
				{
					if(GetResult(AlternativeJsonValue->AsObject(), tmpResult))
					{
						resultsOut.Add(tmpResult);
					}
				}
				return resultsOut.Num() != 0;
			}
			
			if(GetResult(jsonObject, tmpResult))
			{
				resultsOut.Add(tmpResult);
			}
			return resultsOut.Num() != 0;
		}

		UE_LOG(LogSpeechToLife, Error, TEXT("Failed to parse Json!"));
		return false;
	}
}
//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool FSpeechToLifeVoskSession::RunRecognition()
{
	FSpeechToLifeVoskModule& voskModule = FSpeechToLifeVoskModule::Get();
	if(!voskModule.IsVoskLoaded())
	{
		return true;
	}
	
	if(SessionParametersChanged)
	{
		if(VoskRecognizerPtr)
		{
			voskModule.vosk_recognizer_free(VoskRecognizerPtr);
			VoskRecognizerPtr = nullptr;
		}
		if(VoskModelPtr && CurrentLocalID != LocalID)
		{
			// The model changed, free the current
			DereferenceVoskModel(CurrentLocalID);
			VoskModelPtr = nullptr;
		}
		SessionParametersChanged = false;
	}
	if(!VoskRecognizerPtr)
	{
		FScopeLock Lock(&SessionModificationMutex);
// #if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
// 		vosk_set_log_level(3);
// #endif
		
		// Load the model if needed
		if(!VoskModelPtr && !LocalID.IsNone() && !PathToModel.IsEmpty())
		{
			VoskModelPtr = ReferenceVoskModel(LocalID, PathToModel);
			if(VoskModelPtr)
			{
				CurrentLocalID = LocalID;
			}
			else
			{
				UE_LOG(LogSpeechToLifeVosk, Warning, TEXT("FSpeechToLifeVoskSession: Unable to load model '%s'! Recognition will not function."), *PathToModel);
				LocalID = NAME_None;
				PathToModel.Empty();
			}
		}

		if(VoskModelPtr)
		{
			// Create a new recognizer
			VoskRecognizerPtr = voskModule.vosk_recognizer_new(VoskModelPtr, VOSK_INPUT_SAMPLE_RATE);

			// TODO: This is not supported on android yet. We can compile the android libs in the future to include the symbols.
			//if(VoskRecognizerPtr)
			//{
			//	vosk_recognizer_set_words(VoskRecognizerPtr, 1);
			//	vosk_recognizer_set_partial_words(VoskRecognizerPtr, 1);
			//}
		}
	}

	// Just loop if no recognizer. This can happen when we are waiting for a model to be assigned.
	if(!VoskRecognizerPtr)
	{
		// Sleep a bit so we don't hog the SessionModificationMutex waiting for a model
		FPlatformProcess::Sleep(0.5f);
		return true;
	}
	
	FSessionAudioBuffer Buffer;
	while(InputQueue->Dequeue(Buffer))
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		SCOPE_CYCLE_COUNTER(StL_VoskRecognition);
#endif
		IsListening = true;
		
		TArrayView<float> convertedAudio =
			USpeechToLifeFunctionLibrary::ConvertAudioBufferToVoiceAPIRequirement(Buffer,
																		 scratch1,
																		 scratch2,
																		 VOSK_INPUT_SAMPLE_RATE);
		
		// FString strB;
		// for(int32 frameIndex = 0; frameIndex < convertedAudio.Num(); ++frameIndex)
		// {
		// 	strB.Appendf(TEXT("%f,%s"), convertedAudio.GetData()[frameIndex], ((frameIndex % 20) == 19) ? TEXT("\n") : TEXT(" "));
		// }
		// UE_LOG(LogSpeechToLifeVosk, Log, TEXT("%s"), *strB);
		
		const int recognizerResult = voskModule.vosk_recognizer_accept_waveform_f(VoskRecognizerPtr, convertedAudio.GetData(), convertedAudio.Num());
		if (recognizerResult >= 0)
		{
			FString Result;
			if(recognizerResult > 0)
			{
				Result = FString(UTF8_TO_TCHAR(voskModule.vosk_recognizer_final_result(VoskRecognizerPtr)));
			}
			else
			{
				Result = FString(UTF8_TO_TCHAR(voskModule.vosk_recognizer_partial_result(VoskRecognizerPtr)));
			}

			FSessionRecognitionResult resultOut;
			if (VoskSession::ParseJson(Result, resultOut.Results))
			{
				ResultQueue->Enqueue(resultOut);
			}
		}
		else
		{
			UE_LOG(LogSpeechToLifeVosk, Error, TEXT("Vosk: Exception found!"));
		}
	}

	if(IsListening && !ShouldBeListening)
	{
		// Flush last result out from before we stopped listening
		const FString Result = FString(UTF8_TO_TCHAR(voskModule.vosk_recognizer_final_result(VoskRecognizerPtr)));
		FSessionRecognitionResult resultOut;
		if (VoskSession::ParseJson(Result, resultOut.Results))
		{
			ResultQueue->Enqueue(resultOut);
			// Since we got a result we can stop pulling for them
			IsListening = false;
		}
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeVoskSession::CleanupRecognition()
{
	FSpeechToLifeVoskModule& voskModule = FSpeechToLifeVoskModule::Get();
	if(!voskModule.IsVoskLoaded())
	{
		return;
	}
	
	if(VoskRecognizerPtr)
	{
		voskModule.vosk_recognizer_free(VoskRecognizerPtr);
		VoskRecognizerPtr = nullptr;
	}

	if(VoskModelPtr)
	{
		VoskModelPtr = nullptr;
		DereferenceVoskModel(LocalID);
	}
}

// Static defines
FCriticalSection FSpeechToLifeVoskSession::VoskModelsMutex;
TMap<FName, FSpeechToLifeVoskSession::FVoskModelHolder> FSpeechToLifeVoskSession::LoadedVoskModels;

//---------------------------------------------------------------------------------------------------------------------
/**
*/
VoskModel* FSpeechToLifeVoskSession::ReferenceVoskModel(const FName& ID, const FString& pathToModel)
{
	FSpeechToLifeVoskModule& voskModule = FSpeechToLifeVoskModule::Get();
	if(!voskModule.IsVoskLoaded())
	{
		return nullptr;
	}
	
	FScopeLock Lock(&VoskModelsMutex);

	FVoskModelHolder* voskModelHolder = LoadedVoskModels.Find(ID);
	if(voskModelHolder)
	{
		voskModelHolder->RefCount += 1;
		return voskModelHolder->VoskModelPtr;
	}

	UE_LOG(LogSpeechToLife, Log, TEXT("Loading vosk model: %s"), *pathToModel);

	// Try to load the model
	VoskModel* voskModelPtr = voskModule.vosk_model_new(TCHAR_TO_ANSI(*pathToModel));
	if(voskModelPtr)
	{
		FVoskModelHolder holder;
		holder.RefCount = 1;
		holder.VoskModelPtr = voskModelPtr;
		LoadedVoskModels.Add(ID, holder);

		AsyncTask(ENamedThreads::GameThread, [ID]()
		{
			ModelLoadingComplete.Broadcast(ID);
		});
	}

	return voskModelPtr;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeVoskSession::DereferenceVoskModel(const FName& ID)
{
	FSpeechToLifeVoskModule& voskModule = FSpeechToLifeVoskModule::Get();
	if(!voskModule.IsVoskLoaded())
	{
		return;
	}
	
	FScopeLock Lock(&VoskModelsMutex);
	
	FVoskModelHolder* voskModelHolder = LoadedVoskModels.Find(ID);
	if(voskModelHolder)
	{
		voskModelHolder->RefCount -= 1;
		if(voskModelHolder->RefCount == 0)
		{
			voskModule.vosk_model_free(voskModelHolder->VoskModelPtr);
			LoadedVoskModels.Remove(ID);
		}
	}
}
