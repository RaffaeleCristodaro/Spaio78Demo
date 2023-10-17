// Copyright 2020-2023 Solar Storm Interactive

#include "SpeechToLifeSubsystem.h"
#include "SpeechToLifeModule.h"
#include "LatentActions.h"

#if PLATFORM_ANDROID
#include "HAL/PlatformFilemanager.h"
#include "Async/Async.h"
#if USE_ANDROID_JNI
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#endif
#endif

#include "Runtime/Launch/Resources/Version.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
#include "Engine/Engine.h"
#endif

#define LOCTEXT_NAMESPACE "SpeechToLife"

FString SpeechToLifeAndroidNoBackupFilesDir;
bool SpeechToLifeAndroidNoBackupFilesDirValid = false;

#if PLATFORM_ANDROID && USE_ANDROID_JNI
JNI_METHOD void Java_com_epicgames_unreal_GameActivity_nativeSpeechToLifeSetNoBackupFilesDir(JNIEnv* jenv, jobject thiz, jboolean isValid, jstring noBackupFilesDir) 
{
	SpeechToLifeAndroidNoBackupFilesDir = FJavaHelper::FStringFromParam(jenv, noBackupFilesDir);
	SpeechToLifeAndroidNoBackupFilesDirValid = isValid;
	UE_LOG(LogSpeechToLife, Log, TEXT("Models Cache using backup dir: %s"), *SpeechToLifeAndroidNoBackupFilesDir);
}
#endif

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if PLATFORM_ANDROID
	// Delete the old BAD cache directory if it exists
	// NOTE: This is so the speech to life cache doesn't end up in the backup which easily exceeds the devices backup allowance
	extern FString GInternalFilePath;
	const FString folderToDelete = FPaths::Combine(GInternalFilePath, TEXT("SpeechToLife"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if(PlatformFile.DirectoryExists(*folderToDelete))
	{
		// It is here the old stinky! Get rid of it.
		if(PlatformFile.DeleteDirectoryRecursively(*folderToDelete))
		{
			UE_LOG(LogSpeechToLife, Log, TEXT("Deleted old models caching directory: %s"), *folderToDelete);
		}
	}
#endif

	FSpeechToLifeModule& STU = FModuleManager::GetModuleChecked<FSpeechToLifeModule>("SpeechToLife");
	for(int32 localeIndex = 0; localeIndex < Locales.Num(); ++localeIndex)
	{
		// See if the recognizer exists
		FSTULocale& locale = Locales[localeIndex];
		ISpeechToLifeRecognizerPtr recognizer = STU.GetRecognizer(locale.Recognizer);
		if(recognizer)
		{
			// Recognizer exists for this platform, see if the locale folder is around
			FString fullPathToModel;
			if(GetLocaleModelPath(locale, fullPathToModel))
			{
				UE_LOG(LogSpeechToLife, Log, TEXT("Caching locale directory: %s"), *fullPathToModel);
				
				AvailableLocales.Add(localeIndex);
				// Assign the cached full path we have resolved
				locale.ModelFolderPath = fullPathToModel;
#if PLATFORM_ANDROID
				// See if the model is already copied
				if(PlatformFile.DirectoryExists(*locale.ModelFolderPath))
				{
					locale.ModelReady = PlatformFile.FileExists(*FPaths::Combine(locale.ModelFolderPath, TEXT("allgood.txt")));
				}
#else
				// Model is always ready in loose file form for these platforms
				locale.ModelReady = true;
#endif

				if(!locale.ModelReady)
				{
					UE_LOG(LogSpeechToLife, Log, TEXT("Locale '%s' is not ready and will be cached when needed."), *locale.Locale.ToString());
				}
			}
			else
			{
				UE_LOG(LogSpeechToLife, Warning, TEXT("Unable to cache locale '%s' for recognizer '%s'! "
													  "Did you put the model '%s' into 'Content/SpeechToLife/'%s'/models'?"),
													  *locale.Locale.ToString(), *locale.Recognizer.ToString(), *locale.ModelFolderName,
													  *locale.Recognizer.ToString());
			}
		}
	}

	if(!DefaultLocale.IsNone())
	{
		SetLocale(DefaultLocale, true);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

#if PLATFORM_ANDROID
//---------------------------------------------------------------------------------------------------------------------
/**
*/
class FAsyncCopyModelTask : public FNonAbandonableTask
{
	FString Recognizer;
	FString ModelFolder;
	
public:
	FAsyncCopyModelTask(FString recognizer, FString modelFolder)
		: Recognizer(recognizer)
		, ModelFolder(modelFolder)
	{
	}

	void DoWork()
	{
		bool copyComplete = false;
		FString errMessage;
		FString recognizer = Recognizer;
		FString modelFolder = ModelFolder;
		
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString pathTo = FPaths::Combine(SpeechToLifeAndroidNoBackupFilesDir, TEXT("SpeechToLife"), recognizer, TEXT("models"), modelFolder);
		const FString pathFrom = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SpeechToLife"), recognizer, TEXT("models"), modelFolder);

		UE_LOG(LogSpeechToLife, Log, TEXT("Copying model from '%s' to '%s'"), *pathFrom, *pathTo);

		if(PlatformFile.DirectoryExists(*pathTo) || PlatformFile.CreateDirectoryTree(*pathTo))
		{
			if(PlatformFile.DirectoryExists(*pathFrom))
			{
				// A fixed folder visitor that doesn't mess up the path to content.
				// The android version of IterateDirectory is bugged I am guessing because it gets all the paths
				// to iterate but strips the ../../../ part so subsiquant calls fail... Not sure, this is my fix.
				class FModelContentsCopyVisitor : public IPlatformFile::FDirectoryVisitor
				{
					IPlatformFile & PlatformFile;
					TArray<FString>& DirsNext;
					FString DestRootPath;
					
				public:

					FModelContentsCopyVisitor(IPlatformFile& InPlatformFile, TArray<FString>& dirsNext, const FString& destRootPath)
						: PlatformFile(InPlatformFile), DirsNext(dirsNext), DestRootPath(destRootPath)
					{
					}
					
					virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
					{
						const FString filenameOrDirectory = FilenameOrDirectory;
						FString contentRelative;
						filenameOrDirectory.Split(TEXT("/Content/"), nullptr, &contentRelative);
						const FString destPath = FPaths::Combine(DestRootPath, contentRelative);
						FString srcPath = FPaths::Combine(FPaths::ProjectContentDir(), contentRelative);
						
						//UE_LOG(LogSpeechToLife, Log, TEXT("Visiting '%s'"), *srcPath);

						if(bIsDirectory)
						{
							if (!PlatformFile.CreateDirectoryTree(*destPath) && !PlatformFile.DirectoryExists(*destPath))
							{
								UE_LOG(LogSpeechToLife, Log, TEXT("Failed to create model directory '%s'"), *destPath);
								return false;
							}

							//UE_LOG(LogSpeechToLife, Log, TEXT("Created directory '%s'"), *destPath);
							DirsNext.Emplace(MoveTemp(srcPath));
						}
						else
						{
							if(!PlatformFile.FileExists(*destPath))
							{
								// Copy file from source
								if (!PlatformFile.CopyFile(*destPath, *srcPath))
								{
									// Not all files could be copied
									UE_LOG(LogSpeechToLife, Log, TEXT("Failed to copy model file '%s'"), *destPath);
									return false;
								}
								//UE_LOG(LogSpeechToLife, Log, TEXT("Copied model file '%s'"), *destPath);
							}
						}
						return true;
					}
				};

				TArray<FString> directoriesToVisitNext;
				FModelContentsCopyVisitor modelDirVisitor(PlatformFile, directoriesToVisitNext, SpeechToLifeAndroidNoBackupFilesDir);

				copyComplete = true;
				directoriesToVisitNext.Add(pathFrom);
				while(copyComplete && directoriesToVisitNext.Num())
				{
					copyComplete = PlatformFile.IterateDirectory(*directoriesToVisitNext.Pop(), modelDirVisitor);
				}
			}
			else
			{
				UE_LOG(LogSpeechToLife, Log, TEXT("Failed to find model folder '%s' to copy!"), *pathFrom);
			}
		}
		else
		{
			UE_LOG(LogSpeechToLife, Log, TEXT("Unable to create model destination folder '%s' to copy!"), *pathTo);
		}
		
		if(copyComplete)
		{
			IFileHandle* allGoodHandle = PlatformFile.OpenWrite(*FPaths::Combine(pathTo, TEXT("allgood.txt")));
			if(allGoodHandle)
			{
				constexpr uint8 fillerBytes[] = {255};
				allGoodHandle->Write(fillerBytes, 1);
				delete allGoodHandle;
				allGoodHandle = nullptr;
			}
		}
		
		AsyncTask(ENamedThreads::GameThread, [recognizer, modelFolder, copyComplete, errMessage]()
		{
			const UWorld* world = GEngine->GetCurrentPlayWorld();
			if(world && world->GetGameInstance())
			{
				USpeechToLifeSubsystem* speechSubsystem = world->GetGameInstance()->GetSubsystem<USpeechToLifeSubsystem>();
				if(speechSubsystem)
				{
					speechSubsystem->OnModelCopyCB(*recognizer, modelFolder, copyComplete, errMessage);
				}
			}
		});
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCopyModelTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeSubsystem::OnModelCopyCB(const FName recognizer, const FString& modelName, bool copied, const FString& message)
{
	if(copied)
	{
		UE_LOG(LogSpeechToLife, Log, TEXT("Copied model '%s'"), *modelName);
	}
	else
	{
		UE_LOG(LogSpeechToLife, Error, TEXT("Unable to copy model '%s', %s"), *modelName, *message);
	}
	
	for(const int32& availableLocaleIndex : AvailableLocales)
	{
		FSTULocale& availableLocale = Locales[availableLocaleIndex];
		if(availableLocale.Recognizer == recognizer && availableLocale.ModelFolderName == *modelName)
		{
			availableLocale.ModelReady = copied;
			availableLocale.OnLocaleReadyCB.Broadcast(availableLocale.Locale, availableLocale.Recognizer, copied);
			availableLocale.OnLocaleReadyCB.Clear();
			if(SelectedLocaleIndex == availableLocaleIndex && ChangingLocale)
			{
				if(!copied)
				{
					OnSpeechToLifeLocaleChangeFailed.Broadcast(availableLocale.Locale, availableLocale.Recognizer, message);
					OnLocaleChangeComplete(false);
				}
				else
				{
					OnSpeechToLifeLocaleChanged.Broadcast(availableLocale.Locale, availableLocale.Recognizer, availableLocale.ModelFolderPath);
					OnLocaleChangeComplete(true);
				}
			}
		}
	}
}
#endif

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool USpeechToLifeSubsystem::GetLocaleModelPath(const FSTULocale& Entry, FString& pathOut)
{
#if PLATFORM_ANDROID
	// We return the path to use, tho the model might not be copied yet. We do that later as needed.
	pathOut = FPaths::Combine(SpeechToLifeAndroidNoBackupFilesDir, TEXT("SpeechToLife"), Entry.Recognizer.ToString(), TEXT("models"), Entry.ModelFolderName);

	// Check that the model exists in the obb
	FString obbFilePath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SpeechToLife"), Entry.Recognizer.ToString(), TEXT("models"), Entry.ModelFolderName);
	return FPaths::DirectoryExists(obbFilePath);
#else
	pathOut = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("SpeechToLife"), Entry.Recognizer.ToString(), TEXT("models"), Entry.ModelFolderName);
	return FPaths::DirectoryExists(pathOut);
#endif
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
int32 USpeechToLifeSubsystem::GetLocaleIndex(const FName locale, const FName requestedRecognizer) const
{
	int32 selectedLocale = INDEX_NONE;
	int32 currentPriorityWeight = -1;
	
	// Find the locale requested, select an implementation
	for(const int32& availableLocaleIndex : AvailableLocales)
	{
		const FSTULocale& availableLocale = Locales[availableLocaleIndex];
		if(availableLocale.Locale != locale)
		{
			continue;
		}
		
		if(!requestedRecognizer.IsNone() && availableLocale.Recognizer == requestedRecognizer)
		{
			// We found the requested implementation, just use it
			selectedLocale = availableLocaleIndex;
			break;
		}

		if(currentPriorityWeight < availableLocale.LocalePriority)
		{
			selectedLocale = availableLocaleIndex;
			currentPriorityWeight = availableLocale.LocalePriority;
		}
	}

	return selectedLocale;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeSubsystem::GetAvailableLocales(TArray<FSTULocale>& availableLocales) const
{
	availableLocales.Reset();
	for(const int32& localeIndex : AvailableLocales)
	{
		availableLocales.Add(Locales[localeIndex]);
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool USpeechToLifeSubsystem::GetCurrentLocale(FSTULocale& currentLocale) const
{
	if(Locales.IsValidIndex(SelectedLocaleIndex))
	{
		currentLocale = Locales[SelectedLocaleIndex];
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeSubsystem::BP_SetLocale(const UObject* WorldContextObject, const FName locale, const bool blockOnLoad, const FName requestedRecognizer, EStUSetLocaleExecs& OutExecs, FLatentActionInfo LatentInfo)
{
	struct FSetLocaleLatentAction : FPendingLatentAction
	{
		FName ExecutionFunction;
		int32 OutputLink;
		FWeakObjectPtr CallbackTarget;
		FName Locale;
		FName Recognizer;
		bool BlockOnLoad;
		EStUSetLocaleExecs* OutExecs;
		enum stages
		{
			WAITING,
			RUNNING,
			FINISHED
		} CurrentStage;
		int32 MyLocaleChangeRequestUUID;

		FName NextLocale;
		FName NextRecognizer;
		bool NextBlockOnLoad;

		FSetLocaleLatentAction(const FLatentActionInfo& InLatentInfo, const FName locale, const bool blockOnLoad, const FName requestedRecognizer, EStUSetLocaleExecs* outExecs)
			: ExecutionFunction(InLatentInfo.ExecutionFunction)
			, OutputLink(InLatentInfo.Linkage)
			, CallbackTarget(InLatentInfo.CallbackTarget)
			, Locale(locale)
			, Recognizer(requestedRecognizer)
			, BlockOnLoad(blockOnLoad)
			, OutExecs(outExecs)
			, CurrentStage(WAITING)
			, MyLocaleChangeRequestUUID(-1)
			, NextBlockOnLoad(false)
		{
			
		}

		void UpdateRequest(const FName locale, const bool blockOnLoad, const FName requestedRecognizer)
		{
			if(CurrentStage == WAITING)
			{
				Locale = locale;
				BlockOnLoad = blockOnLoad;
				Recognizer = requestedRecognizer;
			}
			else if(CurrentStage == RUNNING)
			{
				NextLocale = locale;
				NextBlockOnLoad = blockOnLoad;
				NextRecognizer = requestedRecognizer;
			}
		}

		virtual void UpdateOperation(FLatentResponse& Response) override
		{
			bool finished = CurrentStage == FINISHED;
			const UWorld* world = GEngine->GetCurrentPlayWorld();
			if(world && world->GetGameInstance())
			{
				USpeechToLifeSubsystem* speechSubsystem = world->GetGameInstance()->GetSubsystem<USpeechToLifeSubsystem>();
				if(speechSubsystem)
				{
					if(CurrentStage == WAITING)
					{
						if(!speechSubsystem->ChangingLocale)
						{
							finished = !speechSubsystem->SetLocale(Locale, BlockOnLoad, Recognizer);
							if(finished)
							{
								// Couldn't change for some reason
								*OutExecs = EStUSetLocaleExecs::Failure;
								CurrentStage = FINISHED;
							}
							else
							{
								if(!speechSubsystem->ChangingLocale)
								{
									// Already finished changing
									*OutExecs = EStUSetLocaleExecs::Success;
									finished = true;
									CurrentStage = FINISHED;
								}
								else
								{
									MyLocaleChangeRequestUUID = speechSubsystem->LocaleChangeUUID;
									CurrentStage = RUNNING;
								}
							}
						}
					}
					else if(CurrentStage == RUNNING)
					{
						// Check if the locale finished changing to
						if(speechSubsystem->LocaleChangeUUID > MyLocaleChangeRequestUUID || !speechSubsystem->ChangingLocale)
						{
							*OutExecs = EStUSetLocaleExecs::Success;
							finished = true;
							CurrentStage = FINISHED;
						}
					}
				}
			}

			if(finished)
			{
				if(!NextLocale.IsNone())
				{
					Locale = NextLocale; NextLocale = NAME_None;
					BlockOnLoad = NextBlockOnLoad;
					Recognizer = NextRecognizer; NextRecognizer = NAME_None;
					CurrentStage = WAITING;
				}
				else
				{
					Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
				}
			}
		}

#if WITH_EDITOR
		virtual FString GetDescription() const override
		{
			switch(CurrentStage)
			{
			case WAITING:
				return FString::Printf(TEXT("Waiting to set locale to: %s"), *Locale.ToString());
			case RUNNING:
				return FString::Printf(TEXT("Setting locale to: %s"), *Locale.ToString());
			default: ;
			}
			return TEXT("Finished!");
		}
#endif
	};

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FSetLocaleLatentAction* currentAction = LatentActionManager.FindExistingAction<FSetLocaleLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
		if (currentAction == nullptr)
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FSetLocaleLatentAction(LatentInfo, locale, blockOnLoad, requestedRecognizer, &OutExecs));
		}
		else
		{
			currentAction->UpdateRequest(locale, blockOnLoad, requestedRecognizer);
		}
	}
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool USpeechToLifeSubsystem::SetLocale(const FName locale, const bool blockOnLoad, const FName requestedRecognizer)
{
	TArray<FSTULocale> availableLocales;
	GetAvailableLocales(availableLocales);
	
	const int32 selectedLocale = GetLocaleIndex(locale, requestedRecognizer);

	if(selectedLocale == INDEX_NONE)
	{
		UE_LOG(LogSpeechToLife, Warning, TEXT("SetLocale: Unable to set locale to '%s'! It either doesn't exist OR it is not setup. "
											  "Check previous logs for warnings about locale caching. Locales need to be defined in "
											  "the project settings under 'SpeechToLife'."), *locale.ToString());
		return false;
	}

	if(selectedLocale == SelectedLocaleIndex)
	{
		UE_LOG(LogSpeechToLife, Log, TEXT("SetLocale ignored because the current locale is already set to '%s'"), *locale.ToString());
		return true;
	}

	if(ChangingLocale)
	{
		NextLocale = locale;
		NextRecognizer = requestedRecognizer;
		UE_LOG(LogSpeechToLife, Warning, TEXT("SetLocale '%s' waiting for previous SetLocale to finish..."), *locale.ToString());
		return true;
	}

	UE_LOG(LogSpeechToLife, Log, TEXT("Requested locale change to '%s'"), *locale.ToString());

	SelectedLocaleIndex = selectedLocale;
	ChangingLocale = true;
	++LocaleChangeUUID;
	OnSpeechToLifeLocaleChanging.Broadcast(Locales[SelectedLocaleIndex].Locale,
							               Locales[SelectedLocaleIndex].Recognizer,
							               Locales[SelectedLocaleIndex].ModelFolderPath);
	
	if(Locales[SelectedLocaleIndex].ModelReady)
	{
		OnLocaleChangeComplete(true);
		OnSpeechToLifeLocaleChanged.Broadcast(Locales[SelectedLocaleIndex].Locale,
							                  Locales[SelectedLocaleIndex].Recognizer,
							                  Locales[SelectedLocaleIndex].ModelFolderPath);
	}
	else
	{
#if PLATFORM_ANDROID
		// Start up the model copy
		if(blockOnLoad)
		{
			FAsyncCopyModelTask copyTask(Locales[SelectedLocaleIndex].Recognizer.ToString(), Locales[SelectedLocaleIndex].ModelFolderName);
			copyTask.DoWork();
		}
		else
		{
			(new FAutoDeleteAsyncTask<FAsyncCopyModelTask>(Locales[SelectedLocaleIndex].Recognizer.ToString(),
														   Locales[SelectedLocaleIndex].ModelFolderName))->StartBackgroundTask();
		}
#else
		// These platforms shouldn't have to copy so getting here is odd.
		OnLocaleChangeComplete(false);
		OnSpeechToLifeLocaleChangeFailed.Broadcast(Locales[SelectedLocaleIndex].Locale, Locales[SelectedLocaleIndex].Recognizer,
												   TEXT("Locale not ready on this platform. Unknown reason."));
		return false;
#endif
	}

	return true;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
TUniquePtr<FSpeechToLifeSession> USpeechToLifeSubsystem::CreateSpeechRecognitionSession(const FName locale, const FName requestedRecognizer) const
{
	if(!locale.IsNone())
	{
		const int32 selectedLocale = GetLocaleIndex(locale, requestedRecognizer);
		if(selectedLocale != INDEX_NONE)
		{
			if(Locales[selectedLocale].ModelReady)
			{
				FSpeechToLifeModule& STU = FModuleManager::GetModuleChecked<FSpeechToLifeModule>("SpeechToLife");
				const ISpeechToLifeRecognizerPtr recognizer = STU.GetRecognizer(Locales[selectedLocale].Recognizer);
				if(recognizer)
				{
					return recognizer->CreateSession(Locales[selectedLocale].Locale, Locales[selectedLocale].ModelFolderPath);
				}
			}
			else
			{
				UE_LOG(LogSpeechToLife, Warning, TEXT("CreateSpeechRecognitionSession: Unable to create session for locale '%s'!"
												      " The locale was not ready to be used. Call MakeLocaleReady first."), *locale.ToString());
			}
		}
		else
		{
			UE_LOG(LogSpeechToLife, Warning, TEXT("CreateSpeechRecognitionSession: Unable to create session for locale '%s'! It either doesn't exist OR it is not setup. "
												  "Check previous logs for warnings about locale caching. Locales need to be defined in "
												  "the project settings under 'SpeechToLife'."), *locale.ToString());
		}
	}
	else if(SelectedLocaleIndex != INDEX_NONE)
	{
		FSpeechToLifeModule& STU = FModuleManager::GetModuleChecked<FSpeechToLifeModule>("SpeechToLife");
		const ISpeechToLifeRecognizerPtr recognizer = STU.GetRecognizer(Locales[SelectedLocaleIndex].Recognizer);
		if(recognizer)
		{
			return recognizer->CreateSession(Locales[SelectedLocaleIndex].Locale, Locales[SelectedLocaleIndex].ModelFolderPath);
		}
	}
	else
	{
		UE_LOG(LogSpeechToLife, Log, TEXT("CreateSpeechRecognitionSession: Unable to create session. Default locale not set and 'locale' param is empty."));
	}

	return nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
bool USpeechToLifeSubsystem::MakeLocaleReady(const FName locale, FOnSpeechToLifeLocaleReady onReadyCB, bool blockOnLoad, const FName requestedRecognizer)
{
	const int32 selectedLocale = GetLocaleIndex(locale, requestedRecognizer);
	if(selectedLocale == INDEX_NONE)
	{
		UE_LOG(LogSpeechToLife, Warning, TEXT("SetLocale: Unable to set locale to '%s'! It either doesn't exist OR it is not setup. "
											  "Check previous logs for warnings about locale caching. Locales need to be defined in "
											  "the project settings under 'SpeechToLife'."), *locale.ToString());
		return false;
	}

	if(Locales[selectedLocale].ModelReady)
	{
		// Broadcast anyway to stay consistent
		onReadyCB.Broadcast(Locales[selectedLocale].Locale, Locales[selectedLocale].Recognizer, true);
		return true;
	}

#if PLATFORM_ANDROID
	Locales[selectedLocale].OnLocaleReadyCB = onReadyCB;
	if(blockOnLoad)
	{
		FAsyncCopyModelTask copyTask(Locales[selectedLocale].Recognizer.ToString(), Locales[selectedLocale].ModelFolderName);
		copyTask.DoWork();
		return Locales[selectedLocale].ModelReady;
	}
	else
	{
		(new FAutoDeleteAsyncTask<FAsyncCopyModelTask>(Locales[selectedLocale].Recognizer.ToString(),
													   Locales[selectedLocale].ModelFolderName))->StartBackgroundTask();
	}
	return true;
#else
	return false;
#endif
}

//---------------------------------------------------------------------------------------------------------------------
/**
*/
void USpeechToLifeSubsystem::OnLocaleChangeComplete(bool success)
{
	ChangingLocale = false;
	if(!NextLocale.IsNone())
	{
		const FName nextLocale = NextLocale; NextLocale = NAME_None;
		const FName nextRecognizer = NextRecognizer; NextRecognizer = NAME_None;
		SetLocale(nextLocale, false, nextRecognizer);
	}
}

#undef LOCTEXT_NAMESPACE
