// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "Components/ActorComponent.h"
#include "SpeechToLifeSession.h"
#include "SpeechToLifeResult.h"
#include "Generators/AudioGenerator.h"
#include "Sound/SoundSubmix.h"
#include "AudioDevice.h"
#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
#include "ISubmixBufferListener.h"
#endif

#include "SpeechToLifeComponent.generated.h"

typedef TFunction<void(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock)> FSubmixDataEventFunction;

/**
 * Submix listener, code mostly copied from NiagaraDataInterfaceAudio.cpp / .h
 * The primary difference is we only listen to submixes that are relavant to the world, not all.
 */
class SPEECHTOLIFE_API FSTUSubmixListener : public ISubmixBufferListener
{
public:
	FSTUSubmixListener(FSubmixDataEventFunction InOnSubmixData, Audio::FDeviceId InDeviceId, USoundSubmix* InSoundSubmix);
	FSTUSubmixListener(FSTUSubmixListener&& Other) noexcept;
	virtual ~FSTUSubmixListener();

	// Begin ISubmixBufferListener overrides
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	// End ISubmixBufferListener overrides

	void RegisterToSubmix();

private:
	FSTUSubmixListener();

	void UnregisterFromSubmix();

	FSubmixDataEventFunction OnSubmixData;

	std::atomic<int32> NumChannelsInSubmix;
	std::atomic<int32> SubmixSampleRate;

	Audio::FDeviceId AudioDeviceId;
	USoundSubmix* Submix;
	bool bIsRegistered;
};


// Primary result delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpeechToLifeResult, const FSTLSpeechResult&, result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSpeechToLifeSessionModelLoaded, const FName, model_ID);

/**
 * A component for taking audio sources (Audio Generator or SubMix) and running voice recognition against it to get
 * recognition results.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SPEECHTOLIFE_API USpeechToLifeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	
	USpeechToLifeComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
		, AudioGenerator(nullptr)
		, IsSubmixListenerRegistered(false)
	{
		PrimaryComponentTick.bCanEverTick = true;
		PrimaryComponentTick.TickInterval = 0.0f;
		PrimaryComponentTick.bStartWithTickEnabled = false;
	}
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	//virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif	  // WITH_EDITOR
	//virtual void PostInitProperties() override;

	virtual void Activate(bool bReset=false) override
	{
		Super::Activate(bReset);
		
		if(RecognitionSession)
		{
			RecognitionSession->StartListening(bReset);
		}
		SetComponentTickEnabled(true);
	}

	virtual void Deactivate() override;

	/// Will return true if a valid recognition session has been created for this SpeechToLife component.
	/// If this is false, there is currently no valid locale set for a session to be created with.
	UFUNCTION(BlueprintPure, Category="SpeechToLife")
	FORCEINLINE bool HasRecognitionSession() const
	{
		return RecognitionSession != nullptr;
	}

	/// This is called per tick by this component when it is active, but it can be called manually if needed for whatever reason.
	UFUNCTION(BlueprintCallable, Category="SpeechToLife")
	void FlushRecognizerResults();

	/// Called as partial results are coming through
	/// These results occur usually mid way through phrase recognition where the recognizer is getting results and is returning them
	/// early.
	UPROPERTY(BlueprintAssignable)
	FSpeechToLifeResult OnPartialResult;

	/// Called when a final result is recognized, and the recognizer believes the speaker has stopped talking.
	UPROPERTY(BlueprintAssignable)
	FSpeechToLifeResult OnFinalResult;

	/// Called when a components speech recognition sessions model has loaded and you can get recognition results with this component.
	UPROPERTY(BlueprintAssignable)
	FSpeechToLifeSessionModelLoaded OnSessionModelLoaded;

	UFUNCTION(BlueprintSetter)
	void SetAudioGenerator(UAudioGenerator* audioGenerator);

	UFUNCTION(BlueprintSetter)
	void SetSubmix(USoundSubmix* submix);

protected:

	UFUNCTION()
	void OnOverrideLocaleReady(const FName locale, const FName recognizer, const bool success);

	/// Allows forcing this component to only use a specific locale.
	/// NOTE: It is better to use the locale set via SpeechToLifeSubsystem.
	UPROPERTY(EditAnywhere, Category = "SpeechToLife", AdvancedDisplay)
	FName LocaleOverride;

	/// Allows forcing this component to specify the recognizer to use with the locale override. Requires LocaleOverride to be set.
	UPROPERTY(EditAnywhere, Category = "SpeechToLife", AdvancedDisplay)
	FName RecognizerOverride;

	/// The audio generator to pull from if set
	UPROPERTY(BlueprintReadWrite, Category = "SpeechToLife", BlueprintSetter=SetAudioGenerator)
	UAudioGenerator* AudioGenerator;

	/// The submix to pull from if set
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpeechToLife", BlueprintSetter=SetSubmix)
	USoundSubmix* Submix;
	
	UFUNCTION()
	void OnSpeechToLifeLocaleChanging(const FName locale, const FName recognizer, const FString modelPath);

	UFUNCTION()
	void OnSpeechToLifeLocaleChanged(const FName locale, const FName recognizer, const FString modelPath);

	// Submix listener code, mostly pulled from NiagaraDataInterfaceAudio.cpp / .h
	void AddSubmixListener(const Audio::FDeviceId InDeviceId);
	void RemoveSubmixListener(const Audio::FDeviceId InDeviceId);
	void RegisterToRelevantAudioDevices();
	void UnregisterFromAllAudioDevices();
	void OnNewDeviceCreated(Audio::FDeviceId InDeviceId);
	void OnDeviceDestroyed(Audio::FDeviceId InDeviceId);

private:

	void OnSessionModelLoadedCB(FName id);

	FCriticalSection RecognitionSessionMutex;

	FAudioGeneratorHandle AudioGeneratorHandle;

	// Created at BeginPlay time, updated if locale changes during runtime.
	TUniquePtr<FSpeechToLifeSession> RecognitionSession;

	// Map of audio devices to submix listeners. Needed for editor where multiple audio devices may exist.
	TMap<Audio::FDeviceId, TUniquePtr<FSTUSubmixListener>> SubmixListeners;

	bool IsSubmixListenerRegistered;
	
	FDelegateHandle DeviceCreatedHandle;
	FDelegateHandle DeviceDestroyedHandle;

	FDelegateHandle ModelLoadingCompleteHandle;
};
