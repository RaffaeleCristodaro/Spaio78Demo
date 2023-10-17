// Copyright 2020-2023 Solar Storm Interactive

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "SpeechToLifeSession.h"
#include "Engine/LatentActionManager.h"

#include "SpeechToLifeSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSpeechToLifeLocaleReady, const FName /* locale */, const FName /* recognizer */, const bool /* success */);

/**
 * A Speech To Life Locale definition
 */
USTRUCT(BlueprintType)
struct FSTULocale
{
	GENERATED_BODY()
	FSTULocale()
	{
		// We only have one right now so just default it and lock it.
		Recognizer = TEXT("vosk");
	}

	/// Which speech recognizer API to use with this locale
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Locale")
	FName Recognizer;

	/// The locale however it is defined for the project (en-us for american english, etc)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locale")
	FName Locale;

	/// The name of the model folder. This is the name of the folder in Content/SpeechToLife/vosk/models/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Locale")
	FString ModelFolderName;

	/// Allows setting a priority value so one recognizer with locale can beat out another
	/// TODO: Not shown yet because without other recognizers it is pretty pointless.
	UPROPERTY(meta=(ClampMin=0, UIMin=0))
	int32 LocalePriority = 0;

	/// The full path to the model folder. Assigned at Initialize time.
	UPROPERTY(BlueprintReadOnly, SkipSerialization, Category="Locale")
	FString ModelFolderPath;

	/// True if the model is ready to be loaded from ModelFolderPath
	UPROPERTY(BlueprintReadOnly, SkipSerialization, Category="Locale")
	bool ModelReady = false;

	// On locale ready. Called if some work needed to be done to make the locale ready.
	FOnSpeechToLifeLocaleReady OnLocaleReadyCB;
};

UENUM(BlueprintType)
enum class EStUSetLocaleExecs : uint8
{
	Success,
	Failure
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSpeechToLifeLocaleChange, const FName, locale, const FName, implementation, const FString, modelPath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSpeechToLifeLocaleChangeFailed, const FName, locale, const FName, implementation, const FString, reason);

/**
 * Speech To Life Subsystem for controlling the current speech recognition locale and creating recognition sessions
 */
UCLASS(BlueprintType, Config = Game, defaultconfig)
class SPEECHTOLIFE_API USpeechToLifeSubsystem : public UGameInstanceSubsystem
{
public:
	GENERATED_BODY()
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	static bool GetLocaleModelPath(const FSTULocale& Entry, FString& pathOut);

	/// The default locale if you would rather SpeechToLife to be set to a specific locale at game session begin.
	/// NOTE: If the application supports multiple languages it would be best to not set this and instead call SetLocale manually
	/// so there isn't an initial model load then another model load for the language the user actually wants to use.
	UPROPERTY(Config, EditAnywhere, Category="SpeechToLife")
	FName DefaultLocale;

	/// Locale definitions
	UPROPERTY(Config, EditAnywhere, Category="SpeechToLife")
	TArray<FSTULocale> Locales;

	/**
	 * Get the index of locale and optionally recognizer combo
	 * @param locale The locale to find
	 * @param requestedRecognizer Optional recognizer to try to use
	 * @return The index in Locales of the found locale. Otherwise INDEX_NONE.
	 */
	 int32 GetLocaleIndex(const FName locale, const FName requestedRecognizer = NAME_None) const;

	/// Get a list of available locales
	UFUNCTION(BlueprintCallable, Category="SpeechToLife")
	void GetAvailableLocales(TArray<FSTULocale>& availableLocales) const;

	/**
	 * Get the currently set locale
	 * @return True if a locale is set and currentLocale has valid data
	 */
	UFUNCTION(BlueprintPure, Category="SpeechToLife")
	bool GetCurrentLocale(FSTULocale& currentLocale) const;

	/// Returns true if the locale is currently changing (being loaded to be used)
	UFUNCTION(BlueprintPure, Category="SpeechToLife")
	FORCEINLINE bool IsChangingLocale() const
	{
		return ChangingLocale;
	}

	/// Returns true if SetLocale was called successfully OR a default locale was set and was setup successfully.
	UFUNCTION(BlueprintPure, Category="SpeechToLife")
	FORCEINLINE bool HasLocaleSet() const
	{
		return SelectedLocaleIndex != INDEX_NONE;
	}
	
	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSetLocale, bool, success);

	/**
	 * Sets the current locale for Speech recognition
	 * HINT: This should be called early in your application startup process.
	 * If changing to a new locale, the current recognition sessions will be torn down and re-started with the new locale.
	 * Expect a wait period while models load etc. You can use OnSpeechToLifeLocaleChanged to know when voice recognition
	 * is back up and running with the new locale.
	 * On PC (windows, linux, osx) platforms this is almost immediate. On Android there is a small wait period to load the model from the APK.
	 * @param WorldContextObject Object to get the world pointer from
	 * @param locale The local to set
	 * @param blockOnLoad Should this call block until finished. Android is the only platform that has a waiting period and can potentially block.
	 * @param requestedRecognizer Optional recognizer to try and use. Overrides all priorities and order.
	 * @param OutExecs The output execution pin selector
	 * @param LatentInfo Generated latent info
	 */
	UFUNCTION(BlueprintCallable, Category="SpeechToLife", meta=(AdvancedDisplay="requestedRecognizer", Latent = "", LatentInfo = "LatentInfo", WorldContext="WorldContextObject", ExpandEnumAsExecs="OutExecs"), DisplayName="SetLocale")
	void BP_SetLocale(const UObject* WorldContextObject, const FName locale, const bool blockOnLoad, const FName requestedRecognizer, EStUSetLocaleExecs& OutExecs, FLatentActionInfo LatentInfo);

	/**
	 * Sets the current locale for Speech recognition
	 * HINT: This should be called early in your application startup process.
	 * If changing to a new locale, the current recognition sessions will be torn down and re-started with the new locale.
	 * Expect a wait period while models load etc. You can use OnSpeechToLifeLocaleChanged to know when voice recognition
	 * is back up and running with the new locale.
	 * On PC (windows, linux, osx) platforms this is almost immediate. On Android there is a small wait period to load the model from the APK.
	 * @param locale The local to set
	 * @param blockOnLoad Should this call block until finished.
	 * @param requestedRecognizer Optional recognizer to try and use. Overrides all priorities and order.
	 */
	bool SetLocale(const FName locale, const bool blockOnLoad = false, const FName requestedRecognizer = NAME_None);

	/// Called when the locale is changing via SetLocale. Depending on the platform, there will be a time period where
	/// files need to be copied. OnSpeechToLifeLocaleChanged will be called after this once the locale is ready.
	UPROPERTY(BlueprintAssignable)
	FOnSpeechToLifeLocaleChange OnSpeechToLifeLocaleChanging;

	/// Called when the new locale has been set and is ready to use by recognizers.
	/// This updates all currently running SpeechToLife components to recognize in the new locale
	UPROPERTY(BlueprintAssignable)
	FOnSpeechToLifeLocaleChange OnSpeechToLifeLocaleChanged;

	/// Called when an error occurs during locale changing. See the log for more details.
	UPROPERTY(BlueprintAssignable)
	FOnSpeechToLifeLocaleChangeFailed OnSpeechToLifeLocaleChangeFailed;

	/// Call to create a session using thee current locale setup
	TUniquePtr<FSpeechToLifeSession> CreateSpeechRecognitionSession(const FName locale = NAME_None, const FName requestedRecognizer = NAME_None) const;

	/// Call to make a specific model ready to use. Best to call this before calling CreateSpeechRecognitionSession
	/// If blockOnLoad is false, use onReadyCB to know when the model is ready, otherwise if this function returns true the model was loaded fine.
	bool MakeLocaleReady(const FName locale, FOnSpeechToLifeLocaleReady onReadyCB, bool blockOnLoad = false, const FName requestedRecognizer = NAME_None);

#if PLATFORM_ANDROID
	void OnModelCopyCB(const FName recognizer, const FString& modelName, bool copied, const FString& message);
#endif

private:

	void OnLocaleChangeComplete(bool success);

	/// Cached list indices into Locales which can be used right now
	TArray<int32> AvailableLocales;

	/// The currently selected locale index.
	/// This will only be set if SetLocale was successful and the model was ready to use.
	int32 SelectedLocaleIndex = INDEX_NONE;

	/// In the middle of changing locale?
	bool ChangingLocale = false;

	/// Cached for when SetLocale is called while in the middle of setting locale
	FName NextLocale;
	FName NextRecognizer;
	int32 LocaleChangeUUID = -1;
};
