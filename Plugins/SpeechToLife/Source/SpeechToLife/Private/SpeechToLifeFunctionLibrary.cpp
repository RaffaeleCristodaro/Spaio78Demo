// Copyright 2020-2023 Solar Storm Interactive

#include "SpeechToLifeFunctionLibrary.h"
#include "SpeechToLifeModule.h"
#include "SpeechToLifeSession.h"
#include "DSP/SampleRateConverter.h"
#include "AudioCaptureExtended.h"

//---------------------------------------------------------------------------------------------------------------------
/**
*/
TArrayView<float> USpeechToLifeFunctionLibrary::ConvertAudioBufferToVoiceAPIRequirement(FSessionAudioBuffer& buffer, TArray<float>& scratchPad, TArray<float>& scratchPad2, const float sampleRate)
{
	scratchPad.Reset();
	scratchPad2.Reset();
	check(sampleRate > 0.0f);
	check(buffer.Frames.Num() > 0);
	check(buffer.Channels > 0 && buffer.Channels <= 2);

	TArrayView<float> arrViewOut(buffer.Frames.GetData(), buffer.Frames.Num());

	// Start by reducing the channels to 1. Do this before rate conversion to reduce the amount of work to do.
	if (buffer.Channels > 1)
	{
		const float* frameData = arrViewOut.GetData();
		const uint32 framesCount = arrViewOut.Num();
		
		if (buffer.Interleaved)
		{
			// Gotta do a bit more work for interleaved, every other frame should be stored
			for (uint32 frameIndex = 0; frameIndex < framesCount; frameIndex += buffer.Channels)
			{
				scratchPad.Add(frameData[frameIndex]);
			}

			arrViewOut = TArrayView<float>(scratchPad.GetData(), scratchPad.Num());
		}
		else
		{
			// Simple enough, just slice the current view in half
			arrViewOut = arrViewOut.Slice(0, arrViewOut.Num() / 2);
		}
	}

	// Convert the rate if needed
	if (buffer.SampleRate != sampleRate)
	{
		const TUniquePtr<Audio::ISampleRateConverter> rateConverter(Audio::ISampleRateConverter::CreateSampleRateConverter());
		rateConverter->Init(buffer.SampleRate / sampleRate, 1);
		rateConverter->ProcessFullbuffer(arrViewOut.GetData(), arrViewOut.Num(), scratchPad2);
		arrViewOut = TArrayView<float>(scratchPad2.GetData(), scratchPad2.Num());
	}

	// Convert to signed 16bit
	for (int32 frameIndex = 0; frameIndex < arrViewOut.Num(); ++frameIndex)
	{
		arrViewOut[frameIndex] = 32767.0f * FMath::Clamp(arrViewOut[frameIndex], -1.0f, 1.0f);
	}
	
	return arrViewOut;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
UAudioCaptureExtended* USpeechToLifeFunctionLibrary::CreateAudioCaptureExtended()
{
	return NewObject<UAudioCaptureExtended>();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeFunctionLibrary::GetAudioCaptureDeviceList(TArray<FSTUCaptureDeviceInfo>& OutAudioCaptureDevices)
{
	OutAudioCaptureDevices.Reset();
	USpeechToLifeFunctionLibrary* audioHelpersCOD = StaticClass()->GetDefaultObject<USpeechToLifeFunctionLibrary>();
	if(audioHelpersCOD)
	{
		if(!audioHelpersCOD->AudioCaptureStream)
		{
			TArray<Audio::IAudioCaptureFactory*> AudioCaptureStreamFactories =
				IModularFeatures::Get().GetModularFeatureImplementations<Audio::IAudioCaptureFactory>(Audio::IAudioCaptureFactory::GetModularFeatureName());

			// For now, just return the first audio capture stream implemented. We can make this configurable at a later point.
			if (AudioCaptureStreamFactories.Num() > 0 && AudioCaptureStreamFactories[0] != nullptr)
			{
				audioHelpersCOD->AudioCaptureStream = AudioCaptureStreamFactories[0]->CreateNewAudioCaptureStream();
			}
		}

		if(audioHelpersCOD->AudioCaptureStream)
		{
			TArray<Audio::FCaptureDeviceInfo> OutDevices;
			audioHelpersCOD->AudioCaptureStream->GetInputDevicesAvailable(OutDevices);
			for(const Audio::FCaptureDeviceInfo& deviceInfo : OutDevices)
			{
				OutAudioCaptureDevices.Emplace(deviceInfo.DeviceName, deviceInfo.DeviceId, deviceInfo.InputChannels, deviceInfo.PreferredSampleRate, deviceInfo.bSupportsHardwareAEC);
			}
		}
		else
		{
			UE_LOG(LogSpeechToLife, Display, TEXT("GetAudioCaptureDeviceList: No Audio Capture implementations found for this platform!"));
		}
	}
}
