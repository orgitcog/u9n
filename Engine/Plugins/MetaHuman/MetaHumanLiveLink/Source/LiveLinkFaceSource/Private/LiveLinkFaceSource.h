// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkClient.h"
#include "ILiveLinkSource.h"

#include "LiveLinkFaceControl.h"
#include "LiveLinkFaceSourceSettings.h"
#include "Common/UdpSocketReceiver.h"

class FLiveLinkFacePacket;
class FLiveLinkFaceControl;
class FSocketDeleter;
class ILiveLinkClient;
class FUdpSocketReceiver;

DECLARE_LOG_CATEGORY_EXTERN(LogLiveLinkFaceSource, Log, All)

class FLiveLinkFaceSource : public ILiveLinkSource, public TSharedFromThis<FLiveLinkFaceSource>
{
public:

	FLiveLinkFaceSource(const FString& InConnectionString);
	virtual ~FLiveLinkFaceSource() override;

	// ~ILiveLinkSource Interface
	
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual bool IsSourceStillValid() const override;
	virtual bool RequestSourceShutdown() override;
	virtual FText GetSourceType() const override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;
	virtual TSubclassOf<ULiveLinkSourceSettings> GetSettingsClass() const override;
	virtual void InitializeSettings(ULiveLinkSourceSettings* InSettings) override;

	// ~ILiveLinkSource Interface

	void Connect(const ULiveLinkFaceSourceSettings* InSettings);
	
	void Stop();

private:

	enum EStatus
	{
		Disconnected = 0,
		Connecting = 1,
		Connected = 2
	};

	FString CustomSubjectName;

	/** The current status of the Live Link source **/
	EStatus Status;

	/** The connection string passed to the source on construction **/
	const FString ConnectionString;

	/** The Control instance used to interact with a remote streaming device */
	TUniquePtr<FLiveLinkFaceControl> Control;

	/** The scaling factor to apply when converting uInt16 control values to float **/
	const float ControlValueScalingFactor;
	
	/** The client used to push Live Link data to the editor */
	ILiveLinkClient* LiveLinkClient = nullptr;

	/** The GUID of the Live Link Source */
	FGuid SourceGuid;

	TSharedPtr<FLiveLinkFaceControl::FRemoteSubject> RemoteSubject;
	
	/** The socket used for receiving multicast UDP packets **/
	TUniquePtr<FSocket, FSocketDeleter> UdpSocket;

	/** Manages the receipt of UDP packets **/
	TUniquePtr<FUdpSocketReceiver> UdpReceiver;
	
	/** Analytics info **/
	bool bSendAnalytics = false;
	double ProcessingStarted = 0;
	int32 NumAnimationFrames = 0;
	TMap<FString, FString> AnalyticsItems;
	FDelegateHandle AnalyticsShutdownHandler;
	void SendAnalytics();

	bool InitUdpReceiver();
	
	void OnDataReceived(const FArrayReaderPtr& InPayload, const FIPv4Endpoint& InEndpoint);
	const FLiveLinkFaceControl::FRemoteSubject OnSelectRemoteSubject(const FLiveLinkFaceControl::FRemoteSubjects& InRemoteSubjects);
	void OnStreamingStarted(const FLiveLinkFaceControl::FRemoteSubject& InRemoteSubject);
	
	void ProcessPacket(const FLiveLinkFacePacket &InPacket);
	void PopulatePropertyValues(const TArray<uint16>& InControlValues, TArray<float>& OutPropertyValues);

	TArray<FLiveLinkSubjectKey> GetSubjects() const;
	void SubjectAdded(FLiveLinkSubjectKey InSubjectKey);

	void PushStaticData(const FLiveLinkSubjectKey &InSubjectKey, const TArray<FName> InPropertyNames);
};
