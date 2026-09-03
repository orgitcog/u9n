// Copyright Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidProfiler.h"

#if USE_ANDROID_JNI
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "Android/AndroidJava.h"
#include "Android/AndroidJavaEnv.h"
#include "Android/AndroidProfiler.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"

DEFINE_LOG_CATEGORY_STATIC(LogAndroidProfiling, Log, Log);

namespace AndroidProfiler
{
	static FAutoConsoleCommand AndroidProfileConsoleCommand(
		TEXT("android.profile"),
		TEXT("Invoke android's profiling api.\n")
		TEXT("Call with no arguments to echo usage to the log.\n")
		TEXT("Note: requires Android 15 or above (API 35).\n")
		TEXT("(this API is rate limited, to remove the rate limit use: 'adb shell device_config put profiling_testing rate_limiter.disabled true')"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
			{
				FString CombinedArgs;
				for (const FString& Argument : Args)
				{
					CombinedArgs += Argument + TEXT(" ");
				}
				FString ProfileName = FAndroidProfiler::StartCapture(CombinedArgs, [](const FAndroidProfiler::FProfileResults& Results)
				{
					if (!Results.FilePath.IsEmpty())
					{
						const FString DestPath = FPaths::ProfilingDir() / FPaths::GetCleanFilename(Results.FilePath);
						UE_CLOG(!Results.Error.IsEmpty(), LogAndroidProfiling, Warning, TEXT("Profile %s completed with log: %s"), *Results.ProfileName, *Results.Error);
						IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
						// move the file to public path so adb can get to it.
						bool bFileMoved = PlatformFile.MoveFile(*DestPath, *Results.FilePath);
						if (!bFileMoved)
						{
							// copy the file if the move fails.
							bFileMoved = PlatformFile.CopyFile(*DestPath, *Results.FilePath);
							PlatformFile.DeleteFile(*Results.FilePath);
						}

						UE_LOG(LogAndroidProfiling, Log, TEXT("Profile %s complete, retrieve via 'adb pull %s'"), *Results.ProfileName, *DestPath);
					}
					else
					{
						UE_LOG(LogAndroidProfiling, Error, TEXT("Profile %s failed: %s"), *Results.ProfileName, *Results.Error);
					}
				});
			}));

	static jclass ProfilerAccessorClassId = 0;
	static jmethodID IssueProfilerCommandMethod = 0;
	static jmethodID StopProfilerCommandMethod = 0;
	static void InitJNI();

	static bool IsJNIAvailable()
	{
		InitJNI();
		return ProfilerAccessorClassId && IssueProfilerCommandMethod && StopProfilerCommandMethod;
	}

	static void InitJNI()
	{
		static bool bInitJNI = false;
		if (bInitJNI)
		{
			return;
		}
		bInitJNI = true;

		if (JNIEnv* Env = AndroidJavaEnv::GetJavaEnv())
		{
			ProfilerAccessorClassId = AndroidJavaEnv::FindJavaClassGlobalRef("com/epicgames/unreal/androidprofiling/ProfilerAccessor");
			if(ProfilerAccessorClassId)
			{
				IssueProfilerCommandMethod = Env->GetStaticMethodID(ProfilerAccessorClassId, "AndroidThunkJava_IssueProfilerCommand", "(Ljava/lang/String;)[Ljava/lang/String;");
				StopProfilerCommandMethod = Env->GetStaticMethodID(ProfilerAccessorClassId, "AndroidThunkJava_StopProfilerCommand", "(Ljava/lang/String;)Z");
			}
		}
		UE_CLOG(!IsJNIAvailable(), LogAndroidProfiling, Error, TEXT("JNI Could not find ProfilerAccessor class or methods. ProfilerAccessor is not supported when Compile SDK < 35."));
	}

	static FString CallProfiler(FString& ProfileNameOUT, const FString& CombinedArgs)
	{
		if (IsJNIAvailable())
		{
			JNIEnv* Env = AndroidJavaEnv::GetJavaEnv();

			auto CombinedArgsJava = FJavaHelper::ToJavaString(Env, CombinedArgs);
			TArray<FString> ProfilerResults = FJavaHelper::ObjectArrayToFStringTArray(Env,
				(jobjectArray)Env->CallStaticObjectMethod(ProfilerAccessorClassId, IssueProfilerCommandMethod, *CombinedArgsJava)
			);

			const bool bException = AndroidJavaEnv::CheckJavaException();
			UE_CLOG(bException, LogAndroidProfiling, Error, TEXT("Profiler failed due to java exception."));

			if(ProfilerResults.Num() == 2)
			{
				ProfileNameOUT = ProfilerResults[0];
				const FString& ProfilerMessage = ProfilerResults[1];
				UE_LOG(LogAndroidProfiling, Log, TEXT("Profiler : %s "), ProfilerMessage.IsEmpty() ? TEXT("profile issued.") : *ProfilerMessage);
				return ProfilerMessage;
			}
			else
			{
				UE_LOG(LogAndroidProfiling, Error, TEXT("Profiler failed."));
			}
		}
		else
		{
			UE_LOG(LogAndroidProfiling, Error, TEXT("Profile failed, java profiler not found."));
		}
		return FString();
	}

	static bool CancelProfile(const FString& ProfileName)
	{
		if( IsJNIAvailable() )
		{
			JNIEnv* Env = AndroidJavaEnv::GetJavaEnv();
			auto ProfileNameJava = FJavaHelper::ToJavaString(Env, ProfileName);
			return (bool)Env->CallStaticBooleanMethod(ProfilerAccessorClassId, StopProfilerCommandMethod, *ProfileNameJava);
		}
		UE_LOG(LogAndroidProfiling, Error, TEXT("Profile cancel failed, java profiler not found."));
		return false;
	}
}

// this provides a static function for accessing FAndroidProfiler just to avoid making OnProfileFinish part of the public api.
struct FAndroidProfilerInternal
{
	static void OnJavaProfileFinish(const FAndroidProfiler::FProfileResults& Results)
	{
		FAndroidProfiler::OnProfileFinish(Results);
	}
};

// called from jvm after profile completes.
JNI_METHOD void Java_com_epicgames_unreal_androidprofiling_ProfilerAccessor_nativeOnProfileFinish(JNIEnv* jenv, jobject /*this*/, jstring ProfileNameJava, jstring ProfileErrorJava, jstring ProfileFilepathJava)
{
	FAndroidProfiler::FProfileResults Results;
	Results.ProfileName = FJavaHelper::FStringFromParam(jenv, ProfileNameJava);
	Results.Error = FJavaHelper::FStringFromParam(jenv, ProfileErrorJava);
	Results.FilePath = FJavaHelper::FStringFromParam(jenv, ProfileFilepathJava);
	UE_LOG(LogAndroidProfiling, Log, TEXT("nativeOnProfileFinish (%s, %s, %s)"),*Results.ProfileName, *Results.Error, *Results.FilePath);
	FAndroidProfilerInternal::OnJavaProfileFinish(Results);
}

// ProfilerCS is held during StartCapture, that completes before we lock and call OnFinish here.
void FAndroidProfiler::OnProfileFinish(const FAndroidProfiler::FProfileResults& Results)
{
	FScopeLock Lock(&ProfilerCS);
	FAndroidProfiler::FActiveSessions FoundSession = ActiveSessions.FindAndRemoveChecked(Results.ProfileName);
	FoundSession.OnFinish(Results);
}

FString FAndroidProfiler::StartCapture(const FString& Args, TUniqueFunction<void(const FAndroidProfiler::FProfileResults& Results)> OnFinish)
{
	FScopeLock Lock(&ProfilerCS);

	FString ProfileName;
	FString Message = AndroidProfiler::CallProfiler(ProfileName, Args);

	if (!ProfileName.IsEmpty())
	{
		check(!ActiveSessions.Contains(ProfileName));
		ActiveSessions.Add(ProfileName, FAndroidProfiler::FActiveSessions({ MoveTemp(OnFinish) }));
	}
	else
	{
		FAndroidProfiler::FProfileResults ProfileNameInProgressError;
		ProfileNameInProgressError.Error = FString::Printf(TEXT("Unable to launch profile. %s"), *Message);
		OnFinish(ProfileNameInProgressError);
	}
	return ProfileName;
}

void FAndroidProfiler::StopCapture(const FString& ProfileName)
{
	FScopeLock Lock(&ProfilerCS);
	if (ActiveSessions.Contains(ProfileName))
	{
		AndroidProfiler::CancelProfile(ProfileName);
	}
	else
	{
		UE_LOG(LogAndroidProfiling, Error, TEXT("StopCapture ignored, profile %s is not active."), *ProfileName);
	}
}
#else

FString FAndroidProfiler::StartCapture(const FString& Args, TUniqueFunction<void(const FAndroidProfiler::FProfileResults& Results)> OnFinish) { return FString(); }
void FAndroidProfiler::StopCapture(const FString& ProfileName) {}
void FAndroidProfiler::OnProfileFinish(const FAndroidProfiler::FProfileResults& Results) {}

#endif
