// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LiveLinkTypes.h"
#include "HAL/Runnable.h"

namespace UE::CaptureManager
{
	class FControlMessenger;
}

DECLARE_LOG_CATEGORY_EXTERN(LogLiveLinkFaceControl, Log, All)

class FLiveLinkFaceControl : public FRunnable
{
public:

	struct FRemoteSubject
	{
		enum class EAnimationType
		{
			ArKit,
			Mha
		};

		FString Id;
		FString Name;
		EAnimationType AnimationType;
		uint16 AnimationVersion;
		TArray<FName> PropertyNames;

		FRemoteSubject(const FString& InId, const FString& InName, const EAnimationType InAnimationType, const uint16 InAnimationVersion, const TArray<FName>& InPropertyNames)
			: Id(InId)
			, Name(InName)
			, AnimationType(InAnimationType)
			, AnimationVersion(InAnimationVersion)
			, PropertyNames(InPropertyNames)
		{}
	};

	using FRemoteSubjects = TArray<FRemoteSubject>;

	DECLARE_DELEGATE_RetVal_OneParam(const FRemoteSubject, FOnSelectRemoteSubject, const FRemoteSubjects&);
	DECLARE_DELEGATE_OneParam(FOnStreamingStarted, const FRemoteSubject&);
	
	FLiveLinkFaceControl(FGuid InSourceGuid, FString InControlHost, uint16 InControlPort, uint16 InStreamPort, FOnSelectRemoteSubject InOnSelectRemoteSubject, FOnStreamingStarted InOnStreamingStarted);
	~FLiveLinkFaceControl();

	void Start();
	
	void RestartConnection();

	const FText& GetServerName() const;
	const FText& GetServerModel() const;
	const FText& GetServerPlatform() const;

	/* FRunnable Implementation Start */
	
	virtual uint32 Run() override;
	virtual void Stop() override;

	/* FRunnable Implementation Stop */
	
private:
	
	bool bIsConnected;
	
	/** Flag indicating that the thread is stopping. */
	bool bStopping;
	
	TUniquePtr<UE::CaptureManager::FControlMessenger> ControlMessenger;

	FText ServerName;
	FText ServerModel;
	FText ServerPlatform;

	const FString Host;

	const uint16 Port;
	
	const FGuid SourceGuid;

	const uint16 StreamPort;

	FRemoteSubjects RemoteSubjects;
	
	const FOnSelectRemoteSubject SelectRemoteSubjectDelegate;
	const FOnStreamingStarted StreamingStartedDelegate;

	TUniquePtr<FRunnableThread> Thread;

	bool StartSession();
	void StopSession();
	
	bool Connect();
	bool StartControlSession();
	bool GetControlServerInformation();
	bool GetSubjects();
	bool StartStreaming(const FRemoteSubject& InRemoteSubject);
	void StopStreaming();
	void StopControlSession();
	void Disconnect();
	
	void OnDisconnect(const FString& InCause);

};
