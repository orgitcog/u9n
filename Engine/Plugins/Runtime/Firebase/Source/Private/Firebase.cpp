// Copyright Epic Games, Inc. All Rights Reserved.

#include "Firebase.h"

DEFINE_LOG_CATEGORY(LogFirebase);

void IFirebaseModuleInterface::StartupModule()
{
}

void IFirebaseModuleInterface::ShutdownModule()
{
}

class FFirebaseModule : public IFirebaseModuleInterface
{
};

IMPLEMENT_MODULE(FFirebaseModule, Firebase);

#if PLATFORM_ANDROID
#include "Android/AndroidJava.h"
#include "Android/AndroidJavaEnv.h"
#include "Android/AndroidJNI.h"
#include "Async/TaskGraphInterfaces.h"

extern "C"
{
	JNIEXPORT void Java_com_epicgames_unreal_notifications_EpicFirebaseMessagingService_OnFirebaseTokenChange(JNIEnv* jenv, jobject thiz, jstring jPreviousToken, jstring jNewToken)
	{
		if (IFirebaseModuleInterface::Get().OnTokenUpdate.IsBound())
		{
			FString PreviousToken = FJavaHelper::FStringFromParam(jenv, jPreviousToken);
			FString NewToken = FJavaHelper::FStringFromParam(jenv, jNewToken);
			FFunctionGraphTask::CreateAndDispatchWhenReady([PreviousToken, NewToken]()
				{
					IFirebaseModuleInterface::Get().OnTokenUpdate.Broadcast(PreviousToken, NewToken);
				}, TStatId(), NULL, ENamedThreads::GameThread);
		}
	}
}
#endif