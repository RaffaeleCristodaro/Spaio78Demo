// Copyright 2020-2023 Solar Storm Interactive

#include "AudioGeneratorSynthComponent.h"
#include "SpeechToLifeModule.h"
#include "DSP/SampleRateConverter.h"

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioGeneratorSynthComponent::SetAudioGenerator(UAudioGenerator* audioGenerator)
{
	if(AudioGenerator)
	{
		AudioGenerator->RemoveGeneratorDelegate(AudioGeneratorHandle);
		AudioGenerator = nullptr;
	}

	AudioGenerator = audioGenerator;
	IsAudioGeneratorInit = false;

	if(AudioGenerator)
	{
		const FOnAudioGenerate audioGeneratorFunc = [this](const float * InAudio, int32 NumSamples)
		{
			if (IsCapturing)
			{
				if(AudioGenerator->GetNumChannels() > 2 || AudioGenerator->GetNumChannels() <= 0)
				{
					if(!IsAudioGeneratorInit)
					{
						UE_LOG(LogSpeechToLife, Warning, TEXT("AudioGeneratorSynthComponent '%s' invalid number of channels, 1 or 2 supported!"),
																	GetOwner() ? *GetOwner()->GetName() : *GetName());
					}
					IsAudioGeneratorInit = true;
					return;
				}
				
				if(!IsAudioGeneratorInit)
				{
					if(AudioGenerator->GetSampleRate() != SampleRateOfAudioGenerator || AudioGenerator->GetNumChannels() != NumChannelsOfAudioGenerator)
					{
						UE_LOG(LogSpeechToLife, Warning, TEXT("AudioGeneratorSynthComponent '%s' output sample rate or channels does not match audio generator! Samples will be converted."),
																	GetOwner() ? *GetOwner()->GetName() : *GetName());
					}
					IsAudioGeneratorInit = true;
				}
				
				TArray<float> scratchPad;
				TArray<float> scratchPad2;
				const float* actualInAudio = InAudio;
				int32 actualNumSamples = NumSamples;

				// Convert channels if needed
				if(AudioGenerator->GetNumChannels() != NumChannelsOfAudioGenerator)
				{
					if(AudioGenerator->GetNumChannels() > NumChannelsOfAudioGenerator)
					{
						scratchPad.Reserve(actualNumSamples / 2);
						// Reduce to 1 channel
						for(int32 sampleIndex = 0; sampleIndex < actualNumSamples; sampleIndex += 2)
						{
							scratchPad.Add(actualInAudio[sampleIndex]);
						}
					}
					else
					{
						scratchPad.Reserve(actualNumSamples * 2);
						// Increase to 2 channels
						for(int32 sampleIndex = 0; sampleIndex < actualNumSamples; ++sampleIndex)
						{
							// Just a copy... Both channels are the same
							scratchPad.Add(actualInAudio[sampleIndex]);
							scratchPad.Add(actualInAudio[sampleIndex]);
						}
					}

					actualInAudio = scratchPad.GetData();
					actualNumSamples = scratchPad.Num();
				}

				// Convert sample rate if needed
				if(AudioGenerator->GetSampleRate() != SampleRateOfAudioGenerator)
				{
					const TUniquePtr<Audio::ISampleRateConverter> rateConverter(Audio::ISampleRateConverter::CreateSampleRateConverter());
					rateConverter->Init(AudioGenerator->GetSampleRate() / SampleRateOfAudioGenerator, NumChannelsOfAudioGenerator);
					rateConverter->ProcessFullbuffer(actualInAudio, actualNumSamples, scratchPad2);
					actualInAudio = scratchPad2.GetData();
					actualNumSamples = scratchPad2.Num();
				}
				
				FScopeLock Lock(&AudioGenerationSection);
				
				// Avoid reading outside of buffer boundaries
				if (GeneratorAudioData.Num() + actualNumSamples <= GetMaxSamples())
				{
					// Append the audio memory to the capture data buffer
					int32 Index = GeneratorAudioData.AddUninitialized(actualNumSamples);
					float* AudioCaptureDataPtr = GeneratorAudioData.GetData();
						
					FMemory::Memcpy(&AudioCaptureDataPtr[Index], actualInAudio, actualNumSamples * sizeof(float));
				}
				else
				{
					UE_LOG(LogSpeechToLife, Warning, TEXT("Attempt to write past end of buffer in OpenDefaultStream [%u]"), GeneratorAudioData.Num() + actualNumSamples);
				}
			}
		};
		
		AudioGeneratorHandle = AudioGenerator->AddGeneratorDelegate(audioGeneratorFunc);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool UAudioGeneratorSynthComponent::Init(int32& SampleRate)
{
	SampleRate = SampleRateOfAudioGenerator;
	NumChannels = NumChannelsOfAudioGenerator;
	CaptureAudioData.Reserve(GetMaxSamples());
	GeneratorAudioData.Reserve(GetMaxSamples());
	return true;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
int32 UAudioGeneratorSynthComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	int32 OutputSamplesGenerated = 0;
	//In case of severe overflow, just drop the data
	if (CaptureAudioData.Num() > GetMaxSamples())
	{
		// Clear the captured data, too
		CaptureAudioData.Reset();
		GetAudioData(CaptureAudioData);
		CaptureAudioData.Reset();
		return NumSamples;
	}
	
	if (CapturedAudioDataSamples > 0 || GetNumSamplesEnqueued() > 1024)
	{
		// Check if we need to read more audio data from capture synth
		if (ReadSampleIndex + NumSamples > CaptureAudioData.Num())
		{
			// but before we do, copy off the remainder of the capture audio data buffer if there's data in it
			int32 SamplesLeft = FMath::Max(0, CaptureAudioData.Num() - ReadSampleIndex);
			if (SamplesLeft > 0)
			{
				float* CaptureDataPtr = CaptureAudioData.GetData();
				if (!(ReadSampleIndex + NumSamples > GetMaxSamples() - 1))
				{
					FMemory::Memcpy(OutAudio, &CaptureDataPtr[ReadSampleIndex], SamplesLeft * sizeof(float));
				}
				else
				{
					UE_LOG(LogAudio, Warning, TEXT("Attempt to write past end of buffer in OnGenerateAudio, when we got more data from the synth"));
					return NumSamples;
				}
				// Track samples generated
				OutputSamplesGenerated += SamplesLeft;
			}
			// Get another block of audio from the capture synth
			CaptureAudioData.Reset();
			GetAudioData(CaptureAudioData);
			// Reset the read sample index since we got a new buffer of audio data
			ReadSampleIndex = 0;
		}
		// note it's possible we didn't get any more audio in our last attempt to get it
		if (CaptureAudioData.Num() > 0)
		{
			// Compute samples to copy
			int32 NumSamplesToCopy = FMath::Min(NumSamples - OutputSamplesGenerated, CaptureAudioData.Num() - ReadSampleIndex);
			float* CaptureDataPtr = CaptureAudioData.GetData();
			if (!(ReadSampleIndex + NumSamplesToCopy > GetMaxSamples() - 1))
			{
				FMemory::Memcpy(&OutAudio[OutputSamplesGenerated], &CaptureDataPtr[ReadSampleIndex], NumSamplesToCopy * sizeof(float));
			}
			else
			{
				UE_LOG(LogAudio, Warning, TEXT("Attempt to read past end of buffer in OnGenerateAudio, when we did not get more data from the synth"));
				return NumSamples;
			}
			ReadSampleIndex += NumSamplesToCopy;
			OutputSamplesGenerated += NumSamplesToCopy;
		}
		CapturedAudioDataSamples += OutputSamplesGenerated;
	}
	else
	{
		// Say we generated the full samples, this will result in silence
		OutputSamplesGenerated = NumSamples;
	}
	return OutputSamplesGenerated;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioGeneratorSynthComponent::OnBeginGenerate()
{
	CapturedAudioDataSamples = 0;
	ReadSampleIndex = 0;
	CaptureAudioData.Reset();
	IsCapturing = true;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void UAudioGeneratorSynthComponent::OnEndGenerate()
{
	FScopeLock Lock(&AudioGenerationSection);
	
	ReadSampleIndex = 0;
	IsCapturing = false;
	GeneratorAudioData.Reset();
	CaptureAudioData.Reset();
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool UAudioGeneratorSynthComponent::GetAudioData(TArray<float>& OutAudioData)
{
	FScopeLock Lock(&AudioGenerationSection);

	int32 CaptureDataSamples = GeneratorAudioData.Num();
	if (CaptureDataSamples > 0)
	{
		// Append the capture audio to the output buffer
		int32 OutIndex = OutAudioData.AddUninitialized(CaptureDataSamples);
		float* OutDataPtr = OutAudioData.GetData();

		//Check bounds of buffer
		if (!(OutIndex > GetMaxSamples()))
		{
			FMemory::Memcpy(&OutDataPtr[OutIndex], GeneratorAudioData.GetData(), CaptureDataSamples * sizeof(float));
		}
		else
		{
			UE_LOG(LogSpeechToLife, Warning, TEXT("Attempt to write past end of buffer in GetAudioData"));
			return false;
		}

		// Reset the capture data buffer since we copied the audio out
		GeneratorAudioData.Reset();
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
int32 UAudioGeneratorSynthComponent::GetNumSamplesEnqueued()
{
	FScopeLock Lock(&AudioGenerationSection);
	return GeneratorAudioData.Num();
}
