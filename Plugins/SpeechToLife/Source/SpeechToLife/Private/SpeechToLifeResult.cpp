// Copyright 2020-2023 Solar Storm Interactive

#include "SpeechToLifeResult.h"
#include "SpeechToLifeModule.h"

#if PLATFORM_ANDROID
#define _MSC_VER 0
#endif
#include "rapidfuzz/fuzz.hpp"
#if PLATFORM_ANDROID
#undef _MSC_VER
#endif

//---------------------------------------------------------------------------------------------------------------------
/**
*/
template<typename T>
static void GetFuzzyMatchSimilarities_T(const FString& sentence, const TArray<FString>& sentencesToCheck, const float scoreCutoff, TArray<float>& sentenceSimilarities)
{
	sentenceSimilarities.Reset();
	const T scorer(*sentence);
	for (const FString& checkSentence : sentencesToCheck)
	{
		const double score = scorer.similarity(*checkSentence, scoreCutoff);
		sentenceSimilarities.Add(score);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeResultFunctionLibrary::GetFuzzyMatchSimilarities(const FString& sentence, const TArray<FString>& sentencesToCheck,
																   const ESTLFuzzyMatchMethod matchType, const float scoreCutoff, TArray<float>& sentenceSimilarities)
{
	switch(matchType)
	{
	default:
	case ESTLFuzzyMatchMethod::Ratio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::PartialRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedPartialRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::TokenSortRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedTokenSortRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::PartialTokenSortRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedPartialTokenSortRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::TokenSetRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedTokenSetRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::PartialTokenSetRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedPartialTokenSetRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::TokenRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedTokenRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::PartialTokenRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedPartialTokenRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::WeightedRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedWRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	case ESTLFuzzyMatchMethod::QuickRatio:
		GetFuzzyMatchSimilarities_T<rapidfuzz::fuzz::CachedQRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceSimilarities);
		break;
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
template<typename T>
void GetFuzzyMatchBestResult_T(const FString& sentence, const TArray<FString>& sentencesToCheck,
							   const float scoreCutoff, int32& sentenceIndexOut, float& similarityOut)
{
	sentenceIndexOut = INDEX_NONE;
	similarityOut = 0.0f;
	const T scorer(*sentence);
	for (int32 sentenceIndex = 0; sentenceIndex < sentencesToCheck.Num(); ++sentenceIndex)
	{
		const FString& checkSentence = sentencesToCheck[sentenceIndex];
		const double score = scorer.similarity(*checkSentence, scoreCutoff);
		if(score > similarityOut)
		{
			sentenceIndexOut = sentenceIndex;
			similarityOut = score;
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeResultFunctionLibrary::GetFuzzyMatchBestResult(const FString& sentence, const TArray<FString>& sentencesToCheck, const ESTLFuzzyMatchMethod matchMethod,
															     const float scoreCutoff, int32& sentenceIndexOut, float& similarityOut)
{
	switch(matchMethod)
	{
	default:
	case ESTLFuzzyMatchMethod::Ratio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::PartialRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedPartialRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::TokenSortRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedTokenSortRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::PartialTokenSortRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedPartialTokenSortRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::TokenSetRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedTokenSetRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::PartialTokenSetRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedPartialTokenSetRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::TokenRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedTokenRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::PartialTokenRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedPartialTokenRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::WeightedRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedWRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	case ESTLFuzzyMatchMethod::QuickRatio:
		GetFuzzyMatchBestResult_T<rapidfuzz::fuzz::CachedQRatio<TCHAR>>(sentence, sentencesToCheck, scoreCutoff, sentenceIndexOut, similarityOut);
		break;
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
float USpeechToLifeResultFunctionLibrary::FuzzyMatchSentences(const FString& sentence1, const FString& sentence2,
															  const ESTLFuzzyMatchMethod matchMethod, const float scoreCutoff)
{
	switch(matchMethod)
	{
	default:
	case ESTLFuzzyMatchMethod::Ratio:
		return rapidfuzz::fuzz::ratio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::PartialRatio:
		return rapidfuzz::fuzz::partial_ratio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::TokenSortRatio:
		return rapidfuzz::fuzz::token_sort_ratio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::PartialTokenSortRatio:
		return rapidfuzz::fuzz::partial_token_sort_ratio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::TokenSetRatio:
		return rapidfuzz::fuzz::token_set_ratio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::PartialTokenSetRatio:
		return rapidfuzz::fuzz::partial_token_set_ratio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::TokenRatio:
		return rapidfuzz::fuzz::token_ratio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::PartialTokenRatio:
		return rapidfuzz::fuzz::partial_token_ratio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::WeightedRatio:
		return rapidfuzz::fuzz::WRatio(*sentence1, *sentence2, scoreCutoff);
	case ESTLFuzzyMatchMethod::QuickRatio:
		return rapidfuzz::fuzz::QRatio(*sentence1, *sentence2, scoreCutoff);
	}
}
