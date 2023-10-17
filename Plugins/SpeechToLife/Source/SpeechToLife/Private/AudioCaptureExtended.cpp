// Copyright 2020-2023 Solar Storm Interactive

#include "AudioCaptureExtended.h"
#include "CoreMinimal.h"
#include "AudioCaptureCore.h"
#include "SpeechToLifeModule.h"
#include "Runtime/Launch/Resources/Version.h"

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UAudioCaptureExtended::UAudioCaptureExtended()
	: SilenceAmplitudeThreshold(0.08f)
	, HasCapturedBeyondSilenceSamples(false)
	, SilenceDetectionAttackTime(2.0f)
	, SilenceDetectionReleaseTime(1100.0f)
	, UseMicrophoneLevelDetector(false)
	, IsLevelDetectorInit(false)
	, bIsCapturingAudio(false)
{
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UAudioCaptureExtended::~UAudioCaptureExtended()
{

}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioCaptureExtended::BeginDestroy()
{
	Super::BeginDestroy();

	bIsCapturingAudio = false;
	
	// In 5+ a crash was introducted in the destructor of the Wasapi capture thread. Explicitly cleaning up the stream fixes this.
#if ENGINE_MAJOR_VERSION >= 5
	if (AudioCapture.IsStreamOpen())
	{
		AudioCapture.CloseStream();
	}
#endif
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioCaptureExtended::SetUseMicrophoneLevelDetector(bool useDetector)
{
	UseMicrophoneLevelDetector = useDetector;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioCaptureExtended::SetCapturingDevice(FName DeviceName)
{
	if(AudioCapture.IsStreamOpen())
	{
		// If already open tear down / reopen using new device
		EnsureOpenAudioStream(DeviceName);
	}
	else
	{
		ChosenDeviceName = DeviceName;
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool UAudioCaptureExtended::EnsureOpenAudioStream(FName DeviceName)
{
	if(AudioCapture.IsStreamOpen())
	{
		if(DeviceName == ChosenDeviceName)
		{
			// Already good!
			return true;
		}
		
		// Tear down the current
		AudioCapture.AbortStream();
		ResetMicrophoneDetection();
	}

	ChosenDeviceName = DeviceName;
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
	Audio::FOnAudioCaptureFunction OnCapture = [this](const void* InAudio, int32 NumFrames, int32 InNumChannels, int32 InSampleRate, double StreamTime, bool bOverFlow)
	{
		const float* AudioData = static_cast<const float*>(InAudio);
#else
	Audio::FOnCaptureFunction OnCapture = [this](const float* AudioData, int32 NumFrames, int32 InNumChannels, int32 InSampleRate, double StreamTime, bool bOverFlow)
	{
#endif
		if(bIsCapturingAudio)
		{
			if(UseMicrophoneLevelDetector)
			{
				if(!IsLevelDetectorInit)
				{
#if ENGINE_MAJOR_VERSION >= 5
					Audio::FEnvelopeFollowerInitParams initParams;
					initParams.SampleRate = InSampleRate;
					initParams.AttackTimeMsec = SilenceDetectionAttackTime;
					initParams.Mode = Audio::EPeakMode::Type::Peak;
					initParams.bIsAnalog = false;
					MicLevelDetector.Init(initParams);
#else
					MicLevelDetector.Init(InSampleRate, SilenceDetectionAttackTime, SilenceDetectionReleaseTime, Audio::EPeakMode::Type::Peak, false);
#endif
					IsLevelDetectorInit = true;
				}
				MicLevelDetector.ProcessAudio(AudioData, NumFrames);
#if ENGINE_MAJOR_VERSION >= 5
				if(MicLevelDetector.GetEnvelopeValues()[0] >= SilenceAmplitudeThreshold)
#else
				if(MicLevelDetector.GetCurrentValue() >= SilenceAmplitudeThreshold)
#endif
				{
					HasCapturedBeyondSilenceSamples = true;
					LastCapturedBeyondSilenceStreamTime = StreamTime;
				}
			}
			OnGeneratedAudio(AudioData, NumFrames * InNumChannels);
		}
	};

	// Start the stream here to avoid hitching the audio render thread. 
	Audio::FAudioCaptureDeviceParams Params;
	// Determine which device
	Audio::FCaptureDeviceInfo deviceInfo;
	AudioCapture.GetCaptureDeviceInfo(deviceInfo);
	if(ChosenDeviceName != NAME_None)
	{
		TArray<Audio::FCaptureDeviceInfo> outDevices;
		AudioCapture.GetCaptureDevicesAvailable(outDevices);
		int32 deviceIndex = 0;
		for(; deviceIndex < outDevices.Num(); ++deviceIndex)
		{
			const Audio::FCaptureDeviceInfo& device = outDevices[deviceIndex];
			if(*device.DeviceName == ChosenDeviceName)
			{
				Params.DeviceIndex = deviceIndex;
				deviceInfo = device;
				break;
			}
		}
		if(deviceIndex >= outDevices.Num())
		{
			UE_LOG(LogSpeechToLife, Warning, TEXT("UAudioCaptureExtended chosen device name is invalid. Using default device!"));
			ChosenDeviceName = NAME_None;
		}
	}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
	if (AudioCapture.OpenAudioCaptureStream(Params, MoveTemp(OnCapture), 1024))
#else
	if (AudioCapture.OpenCaptureStream(Params, MoveTemp(OnCapture), 1024))
#endif
	{
		// If we opened the capture stream succesfully, get the capture device info and initialize the UAudioGenerator
		Init(deviceInfo.PreferredSampleRate, deviceInfo.InputChannels);
		if(bIsCapturingAudio)
		{
			// Already capturing so ensure it is open
			bIsCapturingAudio = AudioCapture.StartStream();
		}
		return true;
	}

	bIsCapturingAudio = false;
	return false;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioCaptureExtended::StartCapturingAudio()
{
	if(EnsureOpenAudioStream(ChosenDeviceName))
	{
		if (!AudioCapture.IsCapturing())
		{
			bIsCapturingAudio = AudioCapture.StartStream();
		}
		else
		{
			bIsCapturingAudio = true;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioCaptureExtended::StopCapturingAudio()
{
	bIsCapturingAudio = false;
	ResetMicrophoneDetection();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool UAudioCaptureExtended::IsCapturingAudio()
{
	return AudioCapture.IsStreamOpen() && AudioCapture.IsCapturing() && bIsCapturingAudio;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioCaptureExtended::CloseCapturingAudio()
{
	if(AudioCapture.IsStreamOpen())
	{
		AudioCapture.AbortStream();
	}
	bIsCapturingAudio = false;
	ResetMicrophoneDetection();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioCaptureExtended::ResetMicrophoneDetection()
{
	LastCapturedBeyondSilenceStreamTime = 0.0f;
	HasCapturedBeyondSilenceSamples = false;
	MicLevelDetector.Reset();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
float UAudioCaptureExtended::GetCurrentAmplitude() const
{
#if ENGINE_MAJOR_VERSION >= 5
	return MicLevelDetector.GetEnvelopeValues()[0];
#else
	return MicLevelDetector.GetCurrentValue();
#endif
}
