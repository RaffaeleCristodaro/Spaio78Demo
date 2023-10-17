// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include "Generators/AudioGenerator.h"

#include "AudioGeneratorSynthComponent.generated.h"

/**
 * A synth component which generates audio from an AudioGenerator object. You can then pass this audio onto an audio device
 * through mixers etc.
 */
UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class SPEECHTOLIFE_API UAudioGeneratorSynthComponent : public USynthComponent
{
	GENERATED_BODY()
public:

	UAudioGeneratorSynthComponent(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
		, SampleRateOfAudioGenerator(48000)
		, NumChannelsOfAudioGenerator(2)
		, NumSecondsToBuffer(5)
		, AudioGenerator(nullptr)
		, IsAudioGeneratorInit(false)
		, CapturedAudioDataSamples(0)
		, IsCapturing(false)
		, ReadSampleIndex(0)
	{
	}

	/// Set the audio generator to feed from
	UFUNCTION(BlueprintCallable, Category="AudioGeneratorSynth")
	void SetAudioGenerator(class UAudioGenerator* audioGenerator);

	// Called when synth is created
	virtual bool Init(int32& SampleRate) override;

	// Called to generate more audio
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

	virtual void OnBeginGenerate() override;
	virtual void OnEndGenerate() override;

	/// Should be set to the sample rate of the audio generator you plan on using. If the sample rate is different
	/// from the audio generator a conversion needs to occur which will hurt synth performance.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (UIMin=16000, UIMax=48000, ClampMin=16000, ClampMax=48000), Category="AudioGeneratorSynth")
	int32 SampleRateOfAudioGenerator;

	/// Should be set to the number of channels of the audio generator you plan on using.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (UIMin=1, UIMax=2, ClampMin=1, ClampMax=2), Category="AudioGeneratorSynth")
	int32 NumChannelsOfAudioGenerator;

	/// How many seconds of audio to buffer from the audio generator before dropping from overrun
	UPROPERTY(BlueprintReadOnly, EditAnywhere, meta = (UIMin=1, UIMax=10, ClampMin=1, ClampMax=10), Category="AudioGeneratorSynth")
	int32 NumSecondsToBuffer;

	FORCEINLINE int32 GetMaxSamples() const
	{
		return SampleRateOfAudioGenerator * NumChannelsOfAudioGenerator * NumSecondsToBuffer;
	}
	
private:
	// Audio Generator
	bool GetAudioData(TArray<float>& OutAudioData);
	int32 GetNumSamplesEnqueued();
	
	UPROPERTY(Transient)
	UAudioGenerator* AudioGenerator;

	bool IsAudioGeneratorInit;
	
	FAudioGeneratorHandle AudioGeneratorHandle;
	TArray<float> GeneratorAudioData;
	FCriticalSection AudioGenerationSection;

	// Audio Synth Out
	TArray<float> CaptureAudioData;
	int32 CapturedAudioDataSamples;
	FThreadSafeBool IsCapturing;
	int32 ReadSampleIndex;
};
