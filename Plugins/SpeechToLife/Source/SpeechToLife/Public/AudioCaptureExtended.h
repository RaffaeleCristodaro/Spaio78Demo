// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "CoreMinimal.h"
#include "Generators/AudioGenerator.h"
#include "AudioCaptureCore.h"

#include "AudioCaptureExtended.generated.h"

/// Class which opens up a handle to an audio capture device.
/// Allows other objects to get audio buffers from the capture device.
/// This is an extended version of Unreals which allows choosing which device to pull from. This is important
/// in modern desktop scenarios where there are many input devices (HMDs, Generic Mics, etc) and the user should choose in the settings
/// which they prefer to use for the games session.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class SPEECHTOLIFE_API UAudioCaptureExtended : public UAudioGenerator
{
	GENERATED_BODY()

public:
	UAudioCaptureExtended();
	virtual ~UAudioCaptureExtended() override;

	virtual void BeginDestroy() override;
	
	/// Set the device to capture to
	/// Calling this is not needed as by default this capture will try with the default device. If you want to call
	/// this, get info by calling GetAudioCaptureDeviceList first to get what device you want to use.
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	void SetCapturingDevice(FName DeviceName);

	/// Returns the device name to use set using SetCapturingDevice. Returns NONE if using default / was not set explicitly.
	UFUNCTION(BlueprintPure, Category = "AudioCapture")
	FName GetChosenDeviceName() const
	{
		return ChosenDeviceName;
	}
	
	/// Starts capturing audio
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	void StartCapturingAudio();

	/// Stops capturing audio
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	void StopCapturingAudio();

	/// Returns true if capturing audio
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	bool IsCapturingAudio();

	/// Closes capturing audio, closes the stream completely.
	//UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	void CloseCapturingAudio();

	/// Resets the state of the microphone detector if UseMicrophoneLevelDetector was on
	UFUNCTION(BlueprintCallable, Category = "AudioCapture")
	void ResetMicrophoneDetection();

	/// Enable this to process the microphone audio and keep a current detected microphone level
	/// This adds extra expense to this generator at runtime.
	UFUNCTION(BlueprintCallable, Category = "AudioCapture|Level Detector")
	void SetUseMicrophoneLevelDetector(bool useDetector);
	
	/// Get the current microphone amplitude. This is only valid (not zero) if UseMicrophoneLevelDetector is true.
	UFUNCTION(BlueprintPure, Category = "AudioCapture|Level Detector")
	float GetCurrentAmplitude() const;
	
	// The Amplitude to check for if UseMicrophoneLevelDetector is true. 
	// If the amplitude meets or exceeds this value LastVoiceCaptureTime will be set to the time this happened
	// and HasCapturedVoiceSamples will be set to true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioCapture|Level Detector")
	float SilenceAmplitudeThreshold;

	/// True if voice samples have been captured at some point
	UPROPERTY(BlueprintReadWrite, Category = "AudioCapture|Level Detector")
	bool HasCapturedBeyondSilenceSamples;

	/// The last stream time non-silence was captured
	UPROPERTY(BlueprintReadWrite, Category = "AudioCapture|Level Detector")
	float LastCapturedBeyondSilenceStreamTime;

	/// The attack time for silence detection used by the microphone detector
	UPROPERTY(BlueprintReadWrite, Category = "AudioCapture|Level Detector")
	float SilenceDetectionAttackTime;

	/// The release time for silence detection used by the microphone detector
	UPROPERTY(BlueprintReadWrite, Category = "AudioCapture|Level Detector")
	float SilenceDetectionReleaseTime;

protected:
	
	bool UseMicrophoneLevelDetector;
	bool IsLevelDetectorInit;
	bool bIsCapturingAudio;

	bool EnsureOpenAudioStream(FName DeviceName);

	// The name of the device we are open to
	FName ChosenDeviceName;

	// The audio capture
	Audio::FAudioCapture AudioCapture;

	// Used for microphone level detection
	Audio::FEnvelopeFollower MicLevelDetector;
};
