// Copyright 2020-2023 Solar Storm Interactive

#include "SpeechToLifeComponent.h"
#include "SpeechToLifeModule.h"
#include "SpeechToLifeSubsystem.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
#include "Engine/World.h"
#endif

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
DECLARE_CYCLE_STAT(TEXT("Component"), StL_Component, STATGROUP_SpeechToLife);
DECLARE_CYCLE_STAT(TEXT("Component Audio Feed"), StL_ComponentAudioFeed, STATGROUP_SpeechToLife);
#endif

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ensure the submix is registered if set
	SetSubmix(Submix);

	// Register to know when models finish loading in the sessions
	ModelLoadingCompleteHandle = FSpeechToLifeSession::ModelLoadingComplete.AddUObject(this, &USpeechToLifeComponent::OnSessionModelLoadedCB);

	check(GetWorld());
	USpeechToLifeSubsystem* stuSubSystem = GetWorld()->GetGameInstance()->GetSubsystem<USpeechToLifeSubsystem>();
	if(stuSubSystem)
	{
		FScopeLock Lock(&RecognitionSessionMutex);
		if(LocaleOverride.IsNone())
		{
			// If the locale changes we want to update our recognizer
			// We only care about this if using the default locale
			stuSubSystem->OnSpeechToLifeLocaleChanging.AddDynamic(this, &USpeechToLifeComponent::OnSpeechToLifeLocaleChanging);
			stuSubSystem->OnSpeechToLifeLocaleChanged.AddDynamic(this, &USpeechToLifeComponent::OnSpeechToLifeLocaleChanged);
		}

		// Try to create a recognition session right away. If we cannot, no locale is set yet, and that is fine.
		if(!LocaleOverride.IsNone())
		{
			FOnSpeechToLifeLocaleReady onLocaleReady;
			onLocaleReady.AddUObject(this, &USpeechToLifeComponent::OnOverrideLocaleReady);
			if(!stuSubSystem->MakeLocaleReady(LocaleOverride, onLocaleReady, false, RecognizerOverride))
			{
				UE_LOG(LogSpeechToLife, Log, TEXT("USpeechToLifeComponent '%s' unable to create a speech recognition session at begin play time"
											  " for locale override '%s'. Check that this locale is setup in the project settings and check the logs for loading warnings."
											  " This component will do nothing."),
											  *GetPathName(), *LocaleOverride.ToString());
			}
		}
		else if(stuSubSystem->HasLocaleSet())
		{
			RecognitionSession = stuSubSystem->CreateSpeechRecognitionSession();
		}
		if(RecognitionSession)
		{
			RecognitionSession->StartSession();
			if(IsActive())
			{
				RecognitionSession->StartListening();
			}
		}
		else
		{
			UE_LOG(LogSpeechToLife, Log, TEXT("USpeechToLifeComponent '%s' unable to create a speech recognition session at begin play time."
			                                  " This is okay but note that recognition cannot occur until a valid locale is set via SpeechToLifeSubsystem::SetLocale()."), *GetPathName());
		}
	}

	DeviceCreatedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddUObject(this, &USpeechToLifeComponent::OnNewDeviceCreated);
	DeviceDestroyedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddUObject(this, &USpeechToLifeComponent::OnDeviceDestroyed);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(ModelLoadingCompleteHandle.IsValid())
	{
		FSpeechToLifeSession::ModelLoadingComplete.Remove(ModelLoadingCompleteHandle);
	}

	if(GetWorld() && GetWorld()->GetGameInstance())
	{
		USpeechToLifeSubsystem* stuSubSystem = GetWorld()->GetGameInstance()->GetSubsystem<USpeechToLifeSubsystem>();
		if(stuSubSystem)
		{
			if(stuSubSystem->OnSpeechToLifeLocaleChanging.IsAlreadyBound(this, &USpeechToLifeComponent::OnSpeechToLifeLocaleChanging))
			{
				stuSubSystem->OnSpeechToLifeLocaleChanging.RemoveDynamic(this, &USpeechToLifeComponent::OnSpeechToLifeLocaleChanging);
				stuSubSystem->OnSpeechToLifeLocaleChanged.RemoveDynamic(this, &USpeechToLifeComponent::OnSpeechToLifeLocaleChanged);
			}
		}
	}

	FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(DeviceCreatedHandle);
	FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(DeviceDestroyedHandle);
	
	if (IsSubmixListenerRegistered)
	{
		UnregisterFromAllAudioDevices();
	}

	if(AudioGenerator)
	{
		AudioGenerator->RemoveGeneratorDelegate(AudioGeneratorHandle);
	}

	FScopeLock Lock(&RecognitionSessionMutex);
	if(RecognitionSession)
	{
		RecognitionSession->TerminateSession();
		RecognitionSession = nullptr;
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::BeginDestroy()
{
	Super::BeginDestroy();

	if(ModelLoadingCompleteHandle.IsValid())
	{
		FSpeechToLifeSession::ModelLoadingComplete.Remove(ModelLoadingCompleteHandle);
	}

	if(GetWorld() && GetWorld()->GetGameInstance())
	{
		USpeechToLifeSubsystem* stuSubSystem = GetWorld()->GetGameInstance()->GetSubsystem<USpeechToLifeSubsystem>();
		if(stuSubSystem)
		{
			if(stuSubSystem->OnSpeechToLifeLocaleChanging.IsAlreadyBound(this, &USpeechToLifeComponent::OnSpeechToLifeLocaleChanging))
			{
				stuSubSystem->OnSpeechToLifeLocaleChanging.RemoveDynamic(this, &USpeechToLifeComponent::OnSpeechToLifeLocaleChanging);
				stuSubSystem->OnSpeechToLifeLocaleChanged.RemoveDynamic(this, &USpeechToLifeComponent::OnSpeechToLifeLocaleChanged);
			}
		}
	}
	
	FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(DeviceCreatedHandle);
	FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(DeviceDestroyedHandle);
	
	if (IsSubmixListenerRegistered)
	{
		UnregisterFromAllAudioDevices();
	}

	if(AudioGenerator)
	{
		AudioGenerator->RemoveGeneratorDelegate(AudioGeneratorHandle);
	}

	FScopeLock Lock(&RecognitionSessionMutex);
	if(RecognitionSession)
	{
		RecognitionSession->TerminateSession();
		RecognitionSession = nullptr;
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Nothing to do if not active
	if(!IsActive())
	{
		return;
	}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	SCOPE_CYCLE_COUNTER(StL_Component);
#endif

	if(Submix)
	{
		if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
		{
			if(GetOwner() && GetOwner()->GetWorld())
			{
				// Update if the main world device changes
				DeviceManager->IterateOverAllDevices([&](Audio::FDeviceId DeviceId, FAudioDevice* InDevice)
				{
					if(GetOwner()->GetWorld()->GetAudioDeviceRaw() == InDevice)
					{
						if(!SubmixListeners.Contains(DeviceId))
						{
							// It changed
							UnregisterFromAllAudioDevices();
							AddSubmixListener(DeviceId);
						}
					}
				});
			}
		}
	}
	
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if(RecognitionSession && !RecognitionSession->GetShouldBeListening())
	{
		UE_LOG(LogSpeechToLife, Error, TEXT("SpeechToLifeComponent is ticking but is not listening. This is a bug in the plugin! Package builds will not work."));
		Deactivate();
		return;
	}
#endif
	
	FlushRecognizerResults();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::Deactivate()
{
	Super::Deactivate();
		
	if(RecognitionSession)
	{
		RecognitionSession->StopListening();
		FlushRecognizerResults();
	}
	SetComponentTickEnabled(false);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::FlushRecognizerResults()
{
	if(RecognitionSession)
	{
		TArray<FSTLSpeechResult> resultsOut;
		RecognitionSession->FetchResults(resultsOut);

		for(const FSTLSpeechResult& result : resultsOut)
		{
			switch(result.Type)
			{
			case ESTLResultType::Partial:
				OnPartialResult.Broadcast(result);
				break;
			case ESTLResultType::Final:
				OnFinalResult.Broadcast(result);
				break;
			default:
				break;
			}
		}
	}
}

#if WITH_EDITOR
//---------------------------------------------------------------------------------------------------------------------
/**

void USpeechToLifeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static FName AudioBusFName = GET_MEMBER_NAME_CHECKED(USpeechToLifeComponent, Submix);
	if (const FProperty* PropertyThatChanged = PropertyChangedEvent.Property)
	{
		if (PropertyThatChanged->GetFName() == AudioBusFName)
		{
			SetSubmix(Submix);
		}
	}
}*/
#endif	  // WITH_EDITOR

//---------------------------------------------------------------------------------------------------------------------
/**

void USpeechToLifeComponent::PostInitProperties()
{
	Super::PostInitProperties();

	SetSubmix(Submix);
}*/

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::SetSubmix(USoundSubmix* submix)
{
	if (IsSubmixListenerRegistered)
	{
		UnregisterFromAllAudioDevices();
		IsSubmixListenerRegistered = false;
	}

	Submix = submix;
	if (!Submix)
	{
		return;
	}

	RegisterToRelevantAudioDevices();
	IsSubmixListenerRegistered = true;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::OnOverrideLocaleReady(const FName locale, const FName recognizer, const bool success)
{
	if(success && GetWorld())
	{
		const USpeechToLifeSubsystem* stuSubSystem = GetWorld()->GetGameInstance()->GetSubsystem<USpeechToLifeSubsystem>();
		if(stuSubSystem)
		{
			FScopeLock Lock(&RecognitionSessionMutex);
			RecognitionSession = stuSubSystem->CreateSpeechRecognitionSession(LocaleOverride, RecognizerOverride);
			if(RecognitionSession)
			{
				RecognitionSession->StartSession();
				if(IsActive())
				{
					RecognitionSession->StartListening();
				}
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::SetAudioGenerator(UAudioGenerator* audioGenerator)
{
	if(AudioGenerator)
	{
		AudioGenerator->RemoveGeneratorDelegate(AudioGeneratorHandle);
		AudioGenerator = nullptr;
	}

	AudioGenerator = audioGenerator;
	if(AudioGenerator)
	{
		const FOnAudioGenerate audioGeneratorFunc = [this](const float * InAudio, int32 NumSamples)
		{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
			SCOPE_CYCLE_COUNTER(StL_ComponentAudioFeed);
#endif
			FScopeLock Lock(&RecognitionSessionMutex);
			if(RecognitionSession && RecognitionSession->GetShouldBeListening())
			{
				FSessionAudioBuffer Buffer;
				Buffer.Interleaved = true;
				Buffer.SampleRate = AudioGenerator->GetSampleRate();
				Buffer.Channels = AudioGenerator->GetNumChannels();
				Buffer.Frames.Reserve(NumSamples);
				Buffer.Frames = MakeArrayView(InAudio, NumSamples); // Only send the first channel
				RecognitionSession->PushAudio(Buffer);
			}
		};
		
		AudioGeneratorHandle = AudioGenerator->AddGeneratorDelegate(audioGeneratorFunc);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::OnSpeechToLifeLocaleChanging(const FName locale, const FName recognizer, const FString modelPath)
{
	if(RecognitionSession)
	{
		FScopeLock Lock(&RecognitionSessionMutex);
		if(RecognitionSession->GetRecognizerID() == recognizer)
		{
			// We can just update the current model, no need to drop our recognizer
			RecognitionSession->ClearModel();
		}
		else
		{
			// We need to re-create the recognizer
			RecognitionSession->TerminateSession();
			RecognitionSession = nullptr;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::OnSpeechToLifeLocaleChanged(const FName locale, const FName recognizer, const FString modelPath)
{
	FScopeLock Lock(&RecognitionSessionMutex);
	if(RecognitionSession)
	{
		RecognitionSession->SetModel(locale, modelPath);
	}
	else if(GetWorld())
	{
		const USpeechToLifeSubsystem* stuSubSystem = GetWorld()->GetGameInstance()->GetSubsystem<USpeechToLifeSubsystem>();
		if(stuSubSystem)
		{
			RecognitionSession = stuSubSystem->CreateSpeechRecognitionSession();
			if(RecognitionSession)
			{
				RecognitionSession->StartSession();
			}
		}
	}

	// If the locale was changed and we are active, start listening
	if(RecognitionSession && IsActive())
	{
		RecognitionSession->StartListening();
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::AddSubmixListener(const Audio::FDeviceId InDeviceId)
{
	check(!SubmixListeners.Contains(InDeviceId));
	SubmixListeners.Emplace(InDeviceId, new FSTUSubmixListener(
	[this](const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)
	{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
SCOPE_CYCLE_COUNTER(StL_ComponentAudioFeed);
#endif
		FScopeLock Lock(&RecognitionSessionMutex);
		if(RecognitionSession && RecognitionSession->GetShouldBeListening())
		{
			FSessionAudioBuffer Buffer;
			Buffer.Interleaved = true;
			Buffer.SampleRate = SampleRate;
			Buffer.Frames.Reserve(NumSamples);
			Buffer.Frames = MakeArrayView(AudioData, NumSamples);
			Buffer.Channels = NumChannels;
			RecognitionSession->PushAudio(Buffer);
		}
	},
	InDeviceId, Submix));
	SubmixListeners[InDeviceId]->RegisterToSubmix();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::RemoveSubmixListener(Audio::FDeviceId InDeviceId)
{
	if (SubmixListeners.Contains(InDeviceId))
	{
		SubmixListeners.Remove(InDeviceId);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::RegisterToRelevantAudioDevices()
{
	if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
	{
		if(GetOwner() && GetOwner()->GetWorld())
		{
			// Register a new submix listener for every audio device that currently exists.
			DeviceManager->IterateOverAllDevices([&](Audio::FDeviceId DeviceId, FAudioDevice* InDevice)
			{
				if(GetOwner()->GetWorld()->GetAudioDeviceRaw() == InDevice)
				{
					AddSubmixListener(DeviceId);
				}
			});
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::UnregisterFromAllAudioDevices()
{
	if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
	{
		// Register a new submix listener for every audio device that currently exists.
		DeviceManager->IterateOverAllDevices([&](Audio::FDeviceId DeviceId, FAudioDevice* InDevice)
		{
			RemoveSubmixListener(DeviceId);
		});
	}

	SubmixListeners.Empty();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::OnNewDeviceCreated(Audio::FDeviceId InDeviceId)
{
	if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
	{
		const FAudioDevice* audioDevice = DeviceManager->GetAudioDeviceRaw(InDeviceId);
		if(audioDevice && GetOwner() && GetOwner()->GetWorld() && GetOwner()->GetWorld()->GetAudioDeviceRaw() == audioDevice)
		{
			UnregisterFromAllAudioDevices();
			AddSubmixListener(InDeviceId);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::OnDeviceDestroyed(Audio::FDeviceId InDeviceId)
{
	RemoveSubmixListener(InDeviceId);
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeComponent::OnSessionModelLoadedCB(FName id)
{
	if(LocaleOverride.IsNone() || LocaleOverride == id)
	{
		OnSessionModelLoaded.Broadcast(id);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
FSTUSubmixListener::FSTUSubmixListener(FSubmixDataEventFunction InOnSubmixData, Audio::FDeviceId InDeviceId, USoundSubmix* InSoundSubmix)
	: OnSubmixData(InOnSubmixData)
	, NumChannelsInSubmix(0)
	, SubmixSampleRate(0)
	, AudioDeviceId(InDeviceId)
	, Submix(InSoundSubmix)
	, bIsRegistered(false)
{
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
FSTUSubmixListener::FSTUSubmixListener()
	: OnSubmixData(nullptr)
	, NumChannelsInSubmix(0)
	, SubmixSampleRate(0)
	, AudioDeviceId(INDEX_NONE)
	, Submix(nullptr)
	, bIsRegistered(false)
{
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
FSTUSubmixListener::FSTUSubmixListener(FSTUSubmixListener&& Other) noexcept
	: FSTUSubmixListener()
{
	UnregisterFromSubmix();
	Other.UnregisterFromSubmix();

	NumChannelsInSubmix = Other.NumChannelsInSubmix.load();
	Other.NumChannelsInSubmix = 0;

	SubmixSampleRate = Other.SubmixSampleRate.load();
	Other.SubmixSampleRate = 0;

	OnSubmixData = Other.OnSubmixData;

	Submix = Other.Submix;
	Other.Submix = nullptr;

	AudioDeviceId = Other.AudioDeviceId;
	Other.AudioDeviceId = INDEX_NONE;

	RegisterToSubmix();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
FSTUSubmixListener::~FSTUSubmixListener()
{
	UnregisterFromSubmix();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSTUSubmixListener::RegisterToSubmix()
{
	if(FAudioDeviceManager::Get())
	{
		FAudioDevice* DeviceHandle = FAudioDeviceManager::Get()->GetAudioDeviceRaw(AudioDeviceId);
		ensure(DeviceHandle);

		if (DeviceHandle)
		{
			DeviceHandle->RegisterSubmixBufferListener(this, Submix);
			bIsRegistered = true;

			// RegisterSubmixBufferListener lazily enqueues the registration on the audio thread,
			// so we have to wait for the audio thread to destroy.
			FAudioCommandFence Fence;
			Fence.BeginFence();
			Fence.Wait();
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSTUSubmixListener::UnregisterFromSubmix()
{
	if(FAudioDeviceManager::Get())
	{
		if (bIsRegistered)
		{
			FAudioDevice* DeviceHandle = FAudioDeviceManager::Get()->GetAudioDeviceRaw(AudioDeviceId);
			if(DeviceHandle)
			{
				DeviceHandle->UnregisterSubmixBufferListener(this, Submix);

				bIsRegistered = false;

				// UnregisterSubmixBufferListener lazily enqueues the unregistration on the audio thread,
				// so we have to wait for the audio thread to destroy.
				FAudioCommandFence Fence;
				Fence.BeginFence();
				Fence.Wait();
			}
			else
			{
				// When terminating a PIE the audio device might already be gone by now
				bIsRegistered = false;
			}
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void FSTUSubmixListener::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)
{
	if(OnSubmixData)
	{
		OnSubmixData(OwningSubmix, AudioData, NumSamples, NumChannels, SampleRate, AudioClock);
	}
}
