// Copyright 2020-2023 Solar Storm Interactive

#include "SpeechToLifeVoskModule.h"
#include "SpeechToLifeModule.h"
#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformProcess.h"

static bool VoskDllLoaded = false;

#if PLATFORM_WINDOWS
#include "Interfaces/IPluginManager.h"
// DLL
static TArray<FString> dllsToLoadOrdered = {TEXT("libwinpthread-1.dll"), TEXT("libgcc_s_seh-1.dll"), TEXT("libstdc++-6.dll"), TEXT("libvosk.dll")};
static TArray<void *> loadedDLLs;

// Function signatures
typedef void(*vosk_recognizer_free_t)(struct VoskRecognizer *recognizer);
typedef struct VoskRecognizer*(*vosk_recognizer_new_t)(struct VoskModel *model, float sample_rate);
typedef int(*vosk_recognizer_accept_waveform_f_t)(struct VoskRecognizer *recognizer, const float *data, int length);
typedef const char *(*vosk_recognizer_final_result_t)(struct VoskRecognizer *recognizer);
typedef const char *(*vosk_recognizer_partial_result_t)(struct VoskRecognizer *recognizer);
typedef struct VoskModel *(*vosk_model_new_t)(const char *model_path);
typedef void (*vosk_model_free_t)(struct VoskModel *model);
// Function pointers
vosk_recognizer_free_t vosk_recognizer_free = nullptr;
vosk_recognizer_new_t vosk_recognizer_new = nullptr;
vosk_recognizer_accept_waveform_f_t vosk_recognizer_accept_waveform_f = nullptr;
vosk_recognizer_final_result_t vosk_recognizer_final_result = nullptr;
vosk_recognizer_partial_result_t vosk_recognizer_partial_result = nullptr;
vosk_model_new_t vosk_model_new = nullptr;
vosk_model_free_t vosk_model_free = nullptr;

#else
#include "vosk_api.h"
#endif

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeVoskModule::StartupModule()
{
#if PLATFORM_WINDOWS
	// On windows we need to link this library dynamically because of a limitation in the engine where it cannot
	// move DLLs into the binaries folder. This is mitigated in engine plugins by including them in the thirdparty folder
	// but we don't have that luxery.
	VoskDllLoaded = false;

	for(const FString& dllFileName : dllsToLoadOrdered)
	{
		const FString Pluginpath = IPluginManager::Get().FindPlugin(TEXT("SpeechToLife"))->GetBaseDir();
		FString DllPath = FPaths::Combine(*Pluginpath, TEXT("Source/ThirdParty/vosk/bin/win64"), *dllFileName);
		UE_LOG(LogSpeechToLife, Log, TEXT("Looking for %s here: %s"), *dllFileName, *DllPath);
		if (!FPaths::FileExists(DllPath))
		{
			DllPath = FPaths::Combine(*Pluginpath, TEXT("Libraries/Win64"), *dllFileName);
			UE_LOG(LogSpeechToLife, Log, TEXT("Looking for %s here: %s"), *dllFileName, *DllPath);
			if (!FPaths::FileExists(DllPath))
			{
				// Could not find...
				UE_LOG(LogSpeechToLife, Error, TEXT("Unable to find %s for vosk voice recognition! vosk can not be used as a recognizer!"), *dllFileName);
				return;
			}
		}

		void* dllHandle = FPlatformProcess::GetDllHandle(*DllPath);
		if (dllHandle == nullptr)
		{
			UE_LOG(LogSpeechToLife, Error, TEXT("Could not load %s for vosk voice recognition! vosk can not be used as a recognizer!"), *dllFileName);
			return;
		}

		loadedDLLs.Add(dllHandle);
	}

	if(loadedDLLs.Num() != dllsToLoadOrdered.Num())
	{
		for(int32 dllHandleIndex = loadedDLLs.Num() - 1; dllHandleIndex >= 0; --dllHandleIndex)
		{
			FPlatformProcess::FreeDllHandle(loadedDLLs[dllHandleIndex]);
			loadedDLLs[dllHandleIndex] = nullptr;
		}
		loadedDLLs.Empty();
		return;
	}

	const int32 dllIndex = dllsToLoadOrdered.Find(TEXT("libvosk.dll"));

	::vosk_recognizer_free = static_cast<vosk_recognizer_free_t>(FPlatformProcess::GetDllExport(loadedDLLs[dllIndex], TEXT("vosk_recognizer_free")));
	::vosk_recognizer_new = static_cast<vosk_recognizer_new_t>(FPlatformProcess::GetDllExport(loadedDLLs[dllIndex], TEXT("vosk_recognizer_new")));
	::vosk_recognizer_accept_waveform_f = static_cast<vosk_recognizer_accept_waveform_f_t>(FPlatformProcess::GetDllExport(loadedDLLs[dllIndex], TEXT("vosk_recognizer_accept_waveform_f")));
	::vosk_recognizer_final_result = static_cast<vosk_recognizer_final_result_t>(FPlatformProcess::GetDllExport(loadedDLLs[dllIndex], TEXT("vosk_recognizer_final_result")));
	::vosk_recognizer_partial_result = static_cast<vosk_recognizer_partial_result_t>(FPlatformProcess::GetDllExport(loadedDLLs[dllIndex], TEXT("vosk_recognizer_partial_result")));
	::vosk_model_new = static_cast<vosk_model_new_t>(FPlatformProcess::GetDllExport(loadedDLLs[dllIndex], TEXT("vosk_model_new")));
	::vosk_model_free = static_cast<vosk_model_free_t>(FPlatformProcess::GetDllExport(loadedDLLs[dllIndex], TEXT("vosk_model_free")));

	VoskDllLoaded = true;

#else
	VoskDllLoaded = true;
#endif
	
	// Register our recognizer with SpeechToLife
	VoskRecognizer = MakeShareable(new FSpeechToLifeVoskRecognizer());
	FSpeechToLifeModule& STU = FModuleManager::GetModuleChecked<FSpeechToLifeModule>("SpeechToLife");
	STU.RegisterRecognizer(VoskRecognizer->GetRecognizerName(), VoskRecognizer);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeVoskModule::ShutdownModule()
{
	VoskRecognizer.Reset();
	
#if PLATFORM_WINDOWS
	if(VoskDllLoaded)
	{
		VoskDllLoaded = false;
		::vosk_recognizer_free = nullptr;
		::vosk_recognizer_new = nullptr;
		::vosk_recognizer_accept_waveform_f = nullptr;
		::vosk_recognizer_final_result = nullptr;
		::vosk_recognizer_partial_result = nullptr;
		::vosk_model_new = nullptr;
		::vosk_model_free = nullptr;
		
		for(int32 dllHandleIndex = loadedDLLs.Num() - 1; dllHandleIndex >= 0; --dllHandleIndex)
		{
			FPlatformProcess::FreeDllHandle(loadedDLLs[dllHandleIndex]);
			loadedDLLs[dllHandleIndex] = nullptr;
		}
		loadedDLLs.Empty();
	}
#endif
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool FSpeechToLifeVoskModule::IsVoskLoaded() const
{
	return VoskDllLoaded;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeVoskModule::vosk_recognizer_free(struct VoskRecognizer* recognizer)
{
	::vosk_recognizer_free(recognizer);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
struct VoskRecognizer* FSpeechToLifeVoskModule::vosk_recognizer_new(VoskModel* model, float sample_rate)
{
	return ::vosk_recognizer_new(model, sample_rate);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
int FSpeechToLifeVoskModule::vosk_recognizer_accept_waveform_f(struct VoskRecognizer* recognizer, const float* data, int length)
{
	return ::vosk_recognizer_accept_waveform_f(recognizer, data, length);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
const char* FSpeechToLifeVoskModule::vosk_recognizer_final_result(struct VoskRecognizer* recognizer)
{
	return ::vosk_recognizer_final_result(recognizer);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
const char* FSpeechToLifeVoskModule::vosk_recognizer_partial_result(struct VoskRecognizer* recognizer)
{
	return ::vosk_recognizer_partial_result(recognizer);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
struct VoskModel* FSpeechToLifeVoskModule::vosk_model_new(const char* model_path)
{
	return ::vosk_model_new(model_path);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSpeechToLifeVoskModule::vosk_model_free(struct VoskModel* model)
{
	::vosk_model_free(model);
}

IMPLEMENT_MODULE(FSpeechToLifeVoskModule, SpeechToLifeVosk);
