// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDRuntimeModule.h"
#include "ChaosVDRecordingDetails.h"
#include "Containers/Ticker.h"
#include "IMessageContext.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"

#include "ChaosVDRemoteSessionsManager.generated.h"

class IMessageContext;
struct FChaosVDSessionPong;
class FMessageEndpoint;
class IMessageBus;

USTRUCT()
struct FChaosVDSessionPing
{
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid ControllerInstanceId;
};

USTRUCT()
struct FChaosVDSessionPong
{
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid InstanceId;
	
	UPROPERTY()
	FGuid SessionId;

	UPROPERTY()
	FString SessionName;

	UPROPERTY()
	uint8 BuildTargetType = static_cast<uint8>(EBuildTargetType::Unknown);
};

USTRUCT()
struct FChaosVDStartRecordingCommandMessage
{
	GENERATED_BODY()

	UPROPERTY()
	EChaosVDRecordingMode RecordingMode = EChaosVDRecordingMode::Invalid;
	
	UPROPERTY()
	FString Target;
};

USTRUCT()
struct FChaosVDStopRecordingCommandMessage
{
	GENERATED_BODY()
};

USTRUCT()
struct FChaosVDRecordingStatusMessage
{
	GENERATED_BODY()
	
	FChaosVDRecordingStatusMessage()
	{
	}

	UPROPERTY()
	FGuid InstanceId = FGuid();
	
	UPROPERTY()
	bool bIsRecording = false;

	UPROPERTY()
	float ElapsedTime = false;
	
	UPROPERTY()
	FChaosVDTraceDetails TraceDetails;
};

USTRUCT()
struct FChaosVDDataChannelState
{
	GENERATED_BODY()

	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	bool bIsEnabled = false;

	UPROPERTY()
	bool bCanChangeChannelState = false;

	bool bWaitingUpdatedState = false;
};

USTRUCT()
struct FChaosVDChannelStateChangeCommandMessage
{
	GENERATED_BODY()
	UPROPERTY()
	FChaosVDDataChannelState NewState;
};

USTRUCT()
struct FChaosVDChannelStateChangeResponseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid InstanceID = FGuid();

	UPROPERTY()
	FChaosVDDataChannelState NewState;
};

USTRUCT()
struct FChaosVDFullSessionInfoRequestMessage
{
	GENERATED_BODY()
};

USTRUCT()
struct FChaosVDFullSessionInfoResponseMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid InstanceId;

	UPROPERTY()
	TArray<FChaosVDDataChannelState> DataChannelsStates;

	UPROPERTY()
	bool bIsRecording = false;
};

UENUM()
enum class EChaosVDRemoteSessionAttributes
{
	None = 0,
	SupportsDataChannelChange = 1 << 0,
	CanExpire = 1 << 1,
	IsMultiSessionWrapper = 1 << 2,
};
ENUM_CLASS_FLAGS(EChaosVDRemoteSessionAttributes)

UENUM()
enum class EChaosVDRemoteSessionReadyState : uint8
{
	/** The session is ready to execute commands */
	Ready,
	/** We are executing a command in the session we expect to take a while without hearing anything from the target. */
	Busy
};

/**
 * Session object that contains all the information needed to communicate with a remote instance, and the state of that instance
 */
struct FChaosVDSessionInfo
{
	explicit FChaosVDSessionInfo() :InstanceId(FGuid()),
									SessionTypeAttributes(EChaosVDRemoteSessionAttributes::CanExpire | EChaosVDRemoteSessionAttributes::SupportsDataChannelChange)
	{
	}
	
	virtual ~FChaosVDSessionInfo() = default;

protected:

	explicit FChaosVDSessionInfo(EChaosVDRemoteSessionAttributes InSessionTypeAttributes) : SessionTypeAttributes(InSessionTypeAttributes)
	{
	}

public:

	FGuid InstanceId;
	FString SessionName;
	FMessageAddress Address;
	FDateTime LastPingTime;
	EBuildTargetType BuildTargetType = EBuildTargetType::Unknown;
	EChaosVDRemoteSessionReadyState ReadyState = EChaosVDRemoteSessionReadyState::Ready;

	FChaosVDRecordingStatusMessage LastKnownRecordingState;
	TMap<FString, FChaosVDDataChannelState> DataChannelsStatesByName;

	virtual bool IsRecording() const;
	
	virtual EChaosVDRecordingMode GetRecordingMode() const;
	
	virtual bool IsConnected() const;

	EChaosVDRemoteSessionAttributes GetSessionTypeAttributes() const
	{
		return SessionTypeAttributes;
	}

protected:
	const EChaosVDRemoteSessionAttributes SessionTypeAttributes;
};

/**
 * Session object that is able to control and provide information about multiple session objects.
 * Used to the UI can use the same API to control multiple session, than for single sessions.
 */
struct FChaosVDMultiSessionInfo : public FChaosVDSessionInfo
{
	explicit FChaosVDMultiSessionInfo() : FChaosVDSessionInfo(EChaosVDRemoteSessionAttributes::IsMultiSessionWrapper)
	{
	}

	virtual ~FChaosVDMultiSessionInfo() override = default;

	virtual bool IsRecording() const override;
	virtual EChaosVDRecordingMode GetRecordingMode() const override;
	virtual bool IsConnected() const override;

	template<typename TCallback>
	void EnumerateInnerSessions(const TCallback& Callback) const
	{
		for (const TPair<FGuid, TWeakPtr<FChaosVDSessionInfo>>& InnerSessionWithID : InnerSessionsByInstanceID)
		{
			if (const TSharedPtr<FChaosVDSessionInfo> SessionPtr = InnerSessionWithID.Value.Pin())
			{
				if (!Callback(SessionPtr.ToSharedRef()))
				{
					return;
				}
			}
		}
	}

	TMap<FGuid, TWeakPtr<FChaosVDSessionInfo>> InnerSessionsByInstanceID;
};

DECLARE_LOG_CATEGORY_EXTERN(LogChaosVDRemoteSession, Log, Log);

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDRecordingStateChangeDelegate, TWeakPtr<FChaosVDSessionInfo> Session)

/** Object that is able to discover, issue and execute commands back and forth between CVD and client/server/editor instances */
class FChaosVDRemoteSessionsManager
{
public:

	FChaosVDRemoteSessionsManager();

	void Initialize();
	void Shutdown();

	FSimpleMulticastDelegate& OnSessionsUpdated()
	{
		return SessionsUpdatedDelegate;
	}


	/**
	 * Issues a command to the provided address that will start a CVD recording
	 * @param InDestinationAddress Message bus address that will execute the command
	 * @param RecordingStartCommandParams Desired parameters to be used to start the recording 
	 */
	CHAOSSOLVERENGINE_API void SendStartRecordingCommand(const FMessageAddress& InDestinationAddress, const FChaosVDStartRecordingCommandMessage& RecordingStartCommandParams);

	/**
	 * Issues a command to the provided address to stop a CVD recording 
	 * 	@param InDestinationAddress Message bus address that will execute the command
	 */
	CHAOSSOLVERENGINE_API void SendStopRecordingCommand(const FMessageAddress& InDestinationAddress);

	/**
	 * Issues a command to the provided address to change the state of a data channel
	 * 	@param InDestinationAddress Message bus address that will execute the command
	 * 	@param InNewStateData New data channel state to apply in the receiving instance
	 */
	CHAOSSOLVERENGINE_API void SendDataChannelStateChangeCommand(const FMessageAddress& InDestinationAddress, const FChaosVDChannelStateChangeCommandMessage& InNewStateData);

	/**
	 * Returns the session info object for the provided ID 
	 * @param Id CVD SessionID
	 */
	CHAOSSOLVERENGINE_API TWeakPtr<FChaosVDSessionInfo> GetSessionInfo(FGuid Id);

	/**
	 * Iterates trough all active and valid cvd sessions, and executes the provided callback to it.
	 * if the callbacks returns false, the iteration will stop
	* */
	template<typename CallbackType>
	void EnumerateActiveSessions(const CallbackType& Callback)
	{
		for (const TPair<FGuid, TSharedPtr<FChaosVDSessionInfo>>& ActiveSession : ActiveSessionsByInstanceId)
		{
			if (ActiveSession.Value)
			{
				bool bContinue = Callback(ActiveSession.Value.ToSharedRef());

				if (!bContinue)
				{
					return;
				}
			}
		}
	}


	/**
	 * Broadcast to the network a recording state update
	 * @param UpdateMessage latest recording state of the issuing instance
	 */
	void PublishRecordingStatusUpdate(const FChaosVDRecordingStatusMessage& UpdateMessage);
	/**
	 * Broadcast to the network a data channel state update
	 * @param InNewStateData latest data channel state of the issuing instance
	 */
	void PublishDataChannelStateChangeUpdate(const FChaosVDChannelStateChangeResponseMessage& InNewStateData);


	/**
	 * Delegate that broadcast when a recording was started in a session (either local or remote)
	 */
	FChaosVDRecordingStateChangeDelegate& OnSessionRecordingStarted()
	{
		return RecordingStartedDelegate;	
	}

	/**
	 * Delegate that broadcast when a recording stops in a session (either local or remote)
	 */
	FChaosVDRecordingStateChangeDelegate& OnSessionRecordingStopped()
	{
		return RecordingStoppedDelegate;
	}
	
	CHAOSSOLVERENGINE_API static const FString AllRemoteSessionsTargetName;
	CHAOSSOLVERENGINE_API static const FString AllRemoteServersTargetName;
	CHAOSSOLVERENGINE_API static const FString AllRemoteClientsTargetName;
	CHAOSSOLVERENGINE_API static const FString AllSessionsTargetName;
	CHAOSSOLVERENGINE_API static const FString CustomSessionsTargetName;
	CHAOSSOLVERENGINE_API static const FString LocalEditorSessionName;
	CHAOSSOLVERENGINE_API static const FName MessageBusEndPointName;
	CHAOSSOLVERENGINE_API static const FGuid AllRemoteSessionsWrapperGUID;
	CHAOSSOLVERENGINE_API static const FGuid AllRemoteServersWrapperGUID;
	CHAOSSOLVERENGINE_API static const FGuid AllRemoteClientsWrapperGUID;
	CHAOSSOLVERENGINE_API static const FGuid AllSessionsWrapperGUID;
	CHAOSSOLVERENGINE_API static const FGuid CustomSessionsWrapperGUID;
	CHAOSSOLVERENGINE_API static const FGuid InvalidSessionGUID;
	CHAOSSOLVERENGINE_API static const FGuid LocalEditorSessionID;

private:
	
	/**
	 * Returns true if this instance has controller capabilities (is either an editor or CVD Standalone, which is also an editor)
	 */
	static constexpr bool IsController();

	/**
	 * Sends a request to obtain the full session information to the provided message bus address
	 * @param InDestinationAddress 
	 */
	void SendFullSessionStateRequestCommand(const FMessageAddress& InDestinationAddress);

	/**
	 * Registers a session object that is able to control multiple session instances
	 * @param InSessionInfoRef Multi Session object
	 */
	void RegisterSessionInMultiSessionWrapper(const TSharedRef<FChaosVDSessionInfo>& InSessionInfoRef);
	
	/**
	 * Deegisters a session object that is able to control multiple session instances
	 * @param InSessionInfoRef Multi Session object
	 */
	void DeRegisterSessionInMultiSessionWrapper(const TSharedRef<FChaosVDSessionInfo>& InSessionInfoRef);

	/**
	 * Creates a session object that is able to control multiple other session objects.
	 * @param InstanceId Instance ID for this object
	 * @param SessionName Session name that will be used in the UI
	 * @return 
	 */
	TSharedPtr<FChaosVDSessionInfo> CreatedWrapperSessionInfo(FGuid InstanceId, const FString& SessionName);

	/***
	 * Creates the messagebus endpoint this session manager will use
	 */
	TSharedPtr<FMessageEndpoint> CreateEndPoint(const TSharedRef<IMessageBus>& InMessageBus);

	bool Tick(float DeltaTime);

	/**
	 * Usually used by the controller. Broadcast to the network this controller exists.
	 */
	void SendPing();

	/**
	 * Broadcast to the network a small subset of this instance information, in to a received session ping.
	 * @param InMessage Ping message that was received
	 */
	void SendPong(const FChaosVDSessionPing& InMessage);
	
	void HandleSessionPongMessage(const FChaosVDSessionPong& InMessage, const TSharedRef<IMessageContext>& InContext);
	void HandleSessionPingMessage(const FChaosVDSessionPing& InMessage, const TSharedRef<IMessageContext>& InContext);
	void HandleRecordingStatusUpdateMessage(const FChaosVDRecordingStatusMessage& InMessage, const TSharedRef<IMessageContext>& InContext);
	void HandleRecordingStartCommandMessage(const FChaosVDStartRecordingCommandMessage& InMessage, const TSharedRef<IMessageContext>& InContext);
	void HandleRecordingStopCommandMessage(const FChaosVDStopRecordingCommandMessage& InMessage, const TSharedRef<IMessageContext>& InContext);
	void HandleChangeDataChannelStateCommandMessage(const FChaosVDChannelStateChangeCommandMessage& InMessage, const TSharedRef<IMessageContext>& InContext);
	void HandleChangeDataChannelStateResponseMessage(const FChaosVDChannelStateChangeResponseMessage& InMessage, const TSharedRef<IMessageContext>& InContext);
	void HandleFullSessionStateRequestMessage(const FChaosVDFullSessionInfoRequestMessage& InMessage, const TSharedRef<IMessageContext>& InContext);
	void HandleFullSessionStateResponseMessage(const FChaosVDFullSessionInfoResponseMessage& InMessage, const TSharedRef<IMessageContext>& InContext);

	void RemoveExpiredSessions();

	/** Holds the time at which the last ping was sent. */
    FDateTime LastPingTime;

    /** Holds a pointer to the message bus. */
    TWeakPtr<IMessageBus> MessageBusPtr;

    /** Holds the messaging endpoint. */
    TSharedPtr<FMessageEndpoint> MessageEndpoint;

	TMap<FGuid, TSharedPtr<FChaosVDSessionInfo>> ActiveSessionsByInstanceId;
	TMap<FGuid, FChaosVDRecordingStatusMessage> PendingRecordingStatusMessages;

	FSimpleMulticastDelegate SessionsUpdatedDelegate;

	FChaosVDRecordingStateChangeDelegate RecordingStartedDelegate;
	FChaosVDRecordingStateChangeDelegate RecordingStoppedDelegate;
	
	FTSTicker::FDelegateHandle TickHandle;
};
