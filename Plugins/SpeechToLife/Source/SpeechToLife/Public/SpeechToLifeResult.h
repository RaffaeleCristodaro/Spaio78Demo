// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "SpeechToLifeResult.generated.h"

/// 
USTRUCT(BlueprintType)
struct FSTLSpeechResultWord
{
	GENERATED_BODY()

	FSTLSpeechResultWord() = default;

	FSTLSpeechResultWord(FString word , float confidence, float startTime, float endTime)
		: Word(word)
		//, Confidence(confidence)
		//, StartTime(startTime)
		//, EndTime(endTime)
	{
	}

	/// The word
	UPROPERTY(BlueprintReadOnly, Category="SpeechResultWord")
	FString Word;

	/// NOTE: These properties below will come into place once we move to an android library which supports words
	/// and confidences in the output. Right now the library we have doesn't contain those symbols.

	/// The confidence that this was the word
	//UPROPERTY(BlueprintReadOnly)
	//float Confidence = 0.0f;

	/// The start time of this word in the phrase/sentence
	//UPROPERTY(BlueprintReadOnly)
	//float StartTime = 0.0f;

	/// The end time of this word in the phrase/sentence
	//UPROPERTY(BlueprintReadOnly)
	//float EndTime = 0.0f;
};

/// The types of results from speech recognition
UENUM(BlueprintType)
enum class ESTLResultType : uint8
{
	/// Not a result
	None,
	/// This is a partial result
	Partial,
	/// This is the final result
	Final,
};

USTRUCT(BlueprintType)
struct FSTLWordIndices
{
	GENERATED_BODY()

	FSTLWordIndices() {}
	FSTLWordIndices(int32 firstIndex)
	{
		WordIndices.Add(firstIndex);
	}

	/// Indices into the Words array of the owning FSTLSpeechResult structure
	UPROPERTY(BlueprintReadOnly, Category="WordIndices")
	TArray<int32> WordIndices;
};

USTRUCT(BlueprintType)
struct FSTLSpeechResult
{
	GENERATED_BODY()

	FSTLSpeechResult()
	{
		Clear();
	}

	/// The type of results
	UPROPERTY(BlueprintReadOnly, Category="SpeechResult")
	ESTLResultType Type;

	/// The sentence recognized overall
	UPROPERTY(BlueprintReadOnly, Category="SpeechResult")
	FString Sentence;

	/// The confidence of the recognized sentence
	UPROPERTY(BlueprintReadOnly, Category="SpeechResult")
	float Confidence;

	/// Each word in order with extra info
	UPROPERTY(BlueprintReadOnly, Category="SpeechResult")
	TArray<FSTLSpeechResultWord> Words;

	// The mapping of words to indices
	UPROPERTY(BlueprintReadOnly, Category="SpeechResult")
	TMap<FString, FSTLWordIndices> WordsMap;
	
	void Clear()
	{
		Type = ESTLResultType::None;
		Confidence = 0.0f;
		Sentence.Reset();
		Words.Reset();
		WordsMap.Reset();
	}

	FORCEINLINE void SplitSentence()
	{
		TArray<FString> words;
		Sentence.ParseIntoArray(words, TEXT(" "), true);
		for(FString& word : words)
		{
			AddWord(word, -1.0f, -1.0f, -1.0f);
		}
	}

	FORCEINLINE void AddWord(const FString& word , float confidence, float startTime, float endTime)
	{
		FSTLWordIndices* indices = WordsMap.Find(word);
		if(indices)
		{
			indices->WordIndices.Add(Words.Add(FSTLSpeechResultWord(word , confidence, startTime, endTime)));
		}
		else
		{
			WordsMap.Add(word, Words.Add(FSTLSpeechResultWord(word , confidence, startTime, endTime)));
		}
	}
};

/// Methods of fuzzy matches
UENUM(BlueprintType)
enum class ESTLFuzzyMatchMethod : uint8
{
	/// calculates a simple ratio between two strings
	/// score is 96.55
	/// double score = Ratio("this is a test", "this is a test!")
	Ratio,
	/// calculates the ratio of the optimal string alignment
	/// score is 100
    /// double score = PartialRatio("this is a test", "this is a test!")
	PartialRatio,
	/// Sorts the words in the strings and calculates the ratio between them
	/// score is 100
	/// double score = TokenSortRatio("fuzzy wuzzy was a bear", "wuzzy fuzzy was a bear")
	TokenSortRatio,
	/// Sorts the words in the strings and calculates the PartialRatio between them
	PartialTokenSortRatio,
	/// Compares the words in the strings based on unique and common words between them using ratio
	/// score1 is 83.87
    /// double score1 = TokenSortRatio("fuzzy was a bear", "fuzzy fuzzy was a bear")
	/// score2 is 100
	/// double score2 = TokenSetRatio("fuzzy was a bear", "fuzzy fuzzy was a bear")
	TokenSetRatio,
	/// Compares the words in the strings based on unique and common words between them using PartialRatio
	PartialTokenSetRatio,

	/// Method that returns the maximum of TokenSetRatio and TokenSortRatio (faster than manually executing the two functions)
	TokenRatio,
	/// Method that returns the maximum of PartialTokenSetRatio and PartialTokenSortRatio (faster than manually executing the two functions)
	PartialTokenRatio,

	/// Calculates a weighted ratio based on the other ratio algorithms
	WeightedRatio,
	/// Calculates a quick ratio between two strings using Ratio
	QuickRatio,
};

/**
 * Speech To Life function library for dealing with recognition results
 */
UCLASS()
class SPEECHTOLIFE_API USpeechToLifeResultFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/// Does this result contain a word
	UFUNCTION(BlueprintPure, Category = "SpeechToLife|Result")
	static FORCEINLINE bool ContainsWord(const FSTLSpeechResult& result, const FString& word)
	{
		return result.WordsMap.Contains(word);
	}

	/// Does this result contain any of the words in the passed in words array
	UFUNCTION(BlueprintPure, Category = "SpeechToLife|Result")
	static FORCEINLINE bool ContainsAnyWords(const FSTLSpeechResult& result, const TArray<FString>& words)
	{
		for (const FString& word : words)
		{
			if (result.WordsMap.Contains(word))
			{
				return true;
			}
		}
		return false;
	}

	/// Does the result contain all words regardless of order?
	UFUNCTION(BlueprintPure, Category = "SpeechToLife|Result")
	static FORCEINLINE bool ContainsAllWords(const FSTLSpeechResult& result, const TArray<FString>& words)
	{
		for (const FString& word : words)
		{
			if (!result.WordsMap.Contains(word))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * Returns true if the result contains the words in order
	 * @param result The result to analize
	 * @param words The words to look for
	 * @param scoreOut The score between 0% and 100%.
	 * @param scoreCutoff The score threshold between 0% and 100%. Passing in 50 means at least 50% of the words need to show up in the words list in order.
	 */
	UFUNCTION(BlueprintPure, Category = "SpeechToLife|Result")
	static FORCEINLINE bool ContainsWordsInOrder(const FSTLSpeechResult& result, const TArray<FString>& words, float& scoreOut, const float scoreCutoff = 100.0f)
	{
		if(!words.Num())
		{
			return false;
		}

		int32 curInnerIndex = 0;
		int32 numWordsFound = 0;
		for (const FString& curWord : words)
		{
			for(int32 checkWordIndex = curInnerIndex; checkWordIndex < result.Words.Num(); ++checkWordIndex)
			{
				if(result.Words[checkWordIndex].Word == curWord)
				{
					// Found the word, continue from this point next word
					++numWordsFound;
					curInnerIndex = checkWordIndex;
					break;
				}
			}
		}
		scoreOut = (static_cast<float>(numWordsFound) / words.Num()) * 100.0f;
		return scoreOut >= scoreCutoff;
	}

	/**
	 * Uses RapidFuzz (https://github.com/maxbachmann/rapidfuzz-cpp) to fuzzy match a sentence vs a number of sentences
	 * @param sentence The sentence to test against sentences
	 * @param sentencesToCheck The sentences tested against sentence
	 * @param matchMethod The method of matching to perform
	 * @param scoreCutoff Optional argument for a score threshold between 0% and 100%. Matches with a lower score than
	 *					  this number will not be returned. Defaults to 0.
	 * @return returns the sentence similaritiees (sentenceSimilarities) of the ratios between sentence and sentencesToCheck or 0 when ratio < score_cutoff.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpeechToLife|Result")
	static void GetFuzzyMatchSimilarities(const FString& sentence, const TArray<FString>& sentencesToCheck, const ESTLFuzzyMatchMethod matchMethod, const float scoreCutoff, TArray<float>& sentenceSimilarities);

	/**
	 * Uses RapidFuzz (https://github.com/maxbachmann/rapidfuzz-cpp) to fuzzy match a sentence vs a number of sentences
	 * @param sentence The sentence to test against sentences
	 * @param sentencesToCheck The sentences tested against sentence
	 * @param matchMethod The method of matching to perform
	 * @param scoreCutoff Optional argument for a score threshold between 0% and 100%. Matches with a lower score than
	 *					  this number will not be returned. Defaults to 0.
	 * @return returns the index (sentenceIndex) of the best ratio between sentence and sentencesToCheck or 0 when ratio < score_cutoff. Also the best similarity between 0 and 100.
	 */
	UFUNCTION(BlueprintCallable, Category = "SpeechToLife|Result")
	static void GetFuzzyMatchBestResult(const FString& sentence, const TArray<FString>& sentencesToCheck, const ESTLFuzzyMatchMethod matchMethod, const float scoreCutoff, int32& sentenceIndex, float& similarity);

	/**
	 * Uses RapidFuzz (https://github.com/maxbachmann/rapidfuzz-cpp) to fuzzy match a sentence vs a sentence
	 * NOTE: If you are testing against a number of strings it is better to use GetFuzzyMatchSimilarities or GetFuzzyMatchBestResult as they are faster
	 *		 because they used a cached result for sentence 1.
	 * @param sentence1 The first sentence to test against the second
	 * @param sentence2 The second sentence tested against the first
	 * @param matchMethod The method of matching to perform
	 * @param scoreCutoff Optional argument for a score threshold between 0% and 100%. Matches with a lower score than
	 *					  this number will not be returned. Defaults to 0.
	 * @return returns the ratio between s1 and s2 or 0 when ratio < score_cutoff
	 */
	UFUNCTION(BlueprintCallable, Category = "SpeechToLife|Result")
	static float FuzzyMatchSentences(const FString& sentence1, const FString& sentence2, const ESTLFuzzyMatchMethod matchMethod, const float scoreCutoff);
};
