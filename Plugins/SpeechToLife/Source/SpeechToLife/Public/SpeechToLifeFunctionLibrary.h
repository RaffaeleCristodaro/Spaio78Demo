// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AudioCaptureDeviceInterface.h"

#include "SpeechToLifeFunctionLibrary.generated.h"

/// A structure inline with Audio::FCaptureDeviceInfo from AudioCaptureCore
USTRUCT(BlueprintType)
struct FSTUCaptureDeviceInfo
{
	GENERATED_BODY()
	
	FSTUCaptureDeviceInfo() = default;

	FSTUCaptureDeviceInfo(const FString& inDeviceName, const FString& inDeviceId, int32 inInputChannels, int32 inPreferredSampleRate, bool inSupportsHardwareAEC)
		: DeviceName(inDeviceName)
		, DeviceId(inDeviceId)
		, InputChannels(inInputChannels)
		, PreferredSampleRate(inPreferredSampleRate)
		, SupportsHardwareAEC(inSupportsHardwareAEC)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="CaptureDeviceInfo")
	FString DeviceName;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="CaptureDeviceInfo")
	FString DeviceId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="CaptureDeviceInfo")
	int32 InputChannels = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="CaptureDeviceInfo")
	int32 PreferredSampleRate = 0;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="CaptureDeviceInfo")
	bool SupportsHardwareAEC = false;
};

/**
 * Speech To Life function library
 */
UCLASS()
class SPEECHTOLIFE_API USpeechToLifeFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static TArrayView<float> ConvertAudioBufferToVoiceAPIRequirement(struct FSessionAudioBuffer& buffer, TArray<float>& scratchPad, TArray<float>& scratchPad2, const float sampleRate);

	/// Create a speech to life audio capture object
	UFUNCTION(BlueprintCallable, Category = "SpeechToLife")
	static class UAudioCaptureExtended* CreateAudioCaptureExtended();

	// Gets a list of available audio capture devices
	UFUNCTION(BlueprintCallable, Category = "SpeechToLife")
	static void GetAudioCaptureDeviceList(TArray<FSTUCaptureDeviceInfo>& OutAudioCaptureDevices);

private:
	TUniquePtr<Audio::IAudioCaptureStream> AudioCaptureStream;
};
