// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialChatChannel.h"
#include "SocialToolkit.h"
#include "SocialReadOnlyChatChannel.h"
#include "SocialPrivateMessageChannel.h"
#include "SocialGroupChannel.h"
#include "SocialChatManager.generated.h"

#define UE_API PARTY_API

class USocialChatRoom;
class USocialPrivateMessageChannel;
class USocialReadOnlyChatChannel;
class USocialUser;
class USocialChatChannel;

enum class ESocialChannelType : uint8;

USTRUCT()
struct FSocialChatChannelConfig
{
	GENERATED_BODY()

	FSocialChatChannelConfig() { SocialUser = nullptr; }

	FSocialChatChannelConfig(class USocialUser* InSocialUser, FString InRoomId, FText InDisplayName, TArray<USocialChatChannel*> InListenChannels = TArray<USocialChatChannel*>())
		: SocialUser(InSocialUser)
		, RoomId(InRoomId)
		, DisplayName(InDisplayName)
	{
		ListenChannels = InListenChannels;
	}

	UPROPERTY()
	TObjectPtr<USocialUser> SocialUser;

	FString RoomId;

	UPROPERTY()
	TArray<TObjectPtr<USocialChatChannel>> ListenChannels;
	FText DisplayName;
};

/** The chat manager is a fully passive construct that watches for creation of chat rooms and message activity therein */
UCLASS(MinimalAPI, Within=SocialToolkit, Config=Game)
class USocialChatManager : public UObject
{
	GENERATED_BODY()

public:
	static UE_API USocialChatManager* CreateChatManager(USocialToolkit& InOwnerToolkit);	
	
	UE_API USocialChatRoom* GetChatRoom(const FChatRoomId& ChannelId) const;
	UE_API virtual void GetJoinedChannels(TArray<USocialChatChannel*>& JoinedChannels) const;

	//void SendDirectMessage(const ISocialUserRef& InRecipient, const FString& InMessage);
	//void SendMessage(const USocialChatChannel& Channel, const FString& InMessage);

	//virtual bool SendMessage(const FString& InChannelName, const FString& InMessage) override;
	//virtual bool SendMessage(const ISocialUserRef& InRecipient, const FString& InMessage) override;

	//virtual void CreateChatRoom(const FChatRoomId& RoomId, const FChatRoomConfig& InChatRoomConfig = FChatRoomConfig()) override;

	//virtual void ConfigureChatRoom(const FChatRoomId& RoomId, const FChatRoomConfig& InChatRoomConfig = FChatRoomConfig(), ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary) override;

	UE_API virtual void JoinChatRoomPublic(const FChatRoomId& RoomId, const FChatRoomConfig& InChatRoomConfig = FChatRoomConfig(), ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary);
	UE_API virtual void JoinChatRoomPrivate(const FChatRoomId& RoomId, const FChatRoomConfig& InChatRoomConfig = FChatRoomConfig(), ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary);

	UE_API virtual void ExitChatRoom(const FChatRoomId& RoomId, ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary);

	DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelCreated, USocialChatChannel&);
	virtual FOnChatChannelCreated& OnChannelCreated() const { return OnChannelCreatedEvent; }

	DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelLeft, USocialChatChannel&);
	virtual FOnChatChannelLeft& OnChannelLeft() const { return OnChannelLeftEvent; }

	/*virtual FOnSocialChannelChanged& OnChatRoomConfigured() override { return OnChatRoomConfiguredEvent; }
	virtual FOnSocialChannelChanged& OnChatRoomJoined() override { return OnChatRoomJoinedEvent; }
	virtual FOnSocialChannelChanged& OnChatRoomExited() override { return OnChatRoomExitedEvent; }*/

	// TODO - Deubanks - Move to Protected here (public version in UFortChatManager once it exists)
	UE_API virtual USocialChatChannel& CreateChatChannel(USocialUser& InRecipient);
	UE_API virtual USocialChatChannel* CreateChatChannel(FSocialChatChannelConfig& InConfig);

	DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelFocusRequested, USocialChatChannel&);
	DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelDisplayRequested, USocialChatChannel&);
	FOnChatChannelFocusRequested& OnChannelFocusRequested() const { return OnChannelFocusRequestedEvent; }
	FOnChatChannelDisplayRequested& OnChannelDisplayRequested() const { return OnChannelDisplayRequestedEvent; }

	UE_API virtual void FocusChatChannel(USocialUser& InChannelUser);
	UE_API virtual void FocusChatChannel(USocialChatChannel& InChannel);
	UE_API virtual void DisplayChatChannel(USocialChatChannel& InChannel);

	UE_API virtual TSubclassOf<USocialChatRoom> GetClassForChatRoom(ESocialChannelType Type) const;
	virtual TSubclassOf<USocialChatChannel> GetClassForPrivateMessage() const { return USocialPrivateMessageChannel::StaticClass(); }

	// @todo - don.eubanks - Maybe move this down into Fort level?
	virtual TSubclassOf<USocialChatChannel> GetClassForReadOnlyChannel() const { return USocialReadOnlyChatChannel::StaticClass(); }

	UE_API virtual bool IsChatRestricted() const;
	virtual TSubclassOf<USocialGroupChannel> GetClassForGroupChannel() const { return USocialGroupChannel::StaticClass(); }

	UE_API USocialToolkit& GetOwningToolkit() const;

	bool AreSlashCommandsEnabled() { return bEnableChatSlashCommands; }

	UE_API USocialChatChannel* GetChatRoomForType(ESocialChannelType Key);




	//----------------------------------------------------------------------
	// KIAROS GROUP MANAGEMENT, tbd channels?

public:

	UE_API virtual void GetGroupChannels(TArray<USocialGroupChannel*>& JoinedChannels) const;

	//DECLARE_EVENT_OneParam(USocialChatManager, FOnChatChannelFocusRequested, USocialChatChannel&);
	//FOnChatChannelFocusRequested& OnGroupsChanged() const { return OnChannelFocusRequestedEvent; }

protected:
	UE_API virtual void InitializeGroupChannels();
	UE_API void LocalUserInitialized(USocialUser& LocalUser);
	UE_API void RefreshGroupsRequestCompleted(FGroupsResult Result);

	UE_API IOnlineGroupsPtr GetOnlineGroupInterface(ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary) const;
	UE_API USocialGroupChannel& FindOrCreateGroupChannel(IOnlineGroupsPtr InGroupInterface, const FUniqueNetId& GroupId);

	UE_API void OnGroupUpdated(const FUniqueNetId& GroupId);

	UE_API bool IsUniqueIdOfOwner(const FUniqueNetId& LocalUserId) const;

	// END KIAROS GROUP MANAGEMENT
	//----------------------------------------------------------------------

protected:
	UE_API IOnlineChatPtr GetOnlineChatInterface(ESocialSubsystem InSocialSubsystem = ESocialSubsystem::Primary) const;
	UE_API virtual void InitializeChatManager();
	UE_API virtual ESocialChannelType TryChannelTypeLookupByRoomId(const FChatRoomId& RoomID);

	UE_API virtual void HandleChatRoomMessageSent(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	UE_API virtual void HandleChatRoomMessageReceived(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const TSharedRef<FChatMessage>& ChatMessage);
	UE_API virtual void HandleChatPrivateMessageSent(const FUniqueNetId& LocalUserId, const FUniqueNetId& RecipientId, bool bWasSuccessful, const FString& Error);
	UE_API virtual void HandleChatPrivateMessageReceived(const FUniqueNetId& LocalUserId, const TSharedRef<FChatMessage>& ChatMessage);

	UE_API virtual void OnChannelCreatedInternal(USocialChatChannel& CreatedChannel);
	UE_API virtual void OnChannelLeftInternal(USocialChatChannel& ChannelLeft);
protected:
	TMap < ESocialChannelType, TWeakObjectPtr<USocialChatChannel>> ChannelsByType;

	UE_API USocialChatRoom& FindOrCreateRoom(const FChatRoomId& RoomId);
	UE_API USocialChatChannel& FindOrCreateChannel(USocialUser& SocialUser);
	UE_API USocialChatChannel& FindOrCreateChannel(const FText& DisplayName);

	UE_API void HandleChatRoomCreated(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	UE_API void HandleChatRoomConfigured(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	UE_API void HandleChatRoomJoinPublic(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	UE_API void HandleChatRoomJoinPrivate(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	UE_API void HandleChatRoomExit(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, bool bWasSuccessful, const FString& Error);
	UE_API void HandleChatRoomMemberJoin(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId);
	UE_API void HandleChatRoomMemberExit(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId);
	UE_API void HandleChatRoomMemberUpdate(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FUniqueNetId& MemberId);

	// Failure handlers (called by HandleXXX functions above)
	virtual void HandleChatRoomCreatedFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }
	virtual void HandleChatRoomConfiguredFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }
	virtual void HandleChatRoomJoinPublicFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }
	virtual void HandleChatRoomJoinPrivateFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }
	virtual void HandleChatRoomExitFailure(const FUniqueNetId& LocalUserId, const FChatRoomId& RoomId, const FString& Error) { }

protected:
	UPROPERTY()
	TMap<TWeakObjectPtr<USocialUser>, TObjectPtr<USocialPrivateMessageChannel>> DirectChannelsByTargetUser;

	UPROPERTY()
	TMap<FString, TObjectPtr<USocialChatRoom>> ChatRoomsById;

	UPROPERTY()
	TMap<FString, TObjectPtr<USocialReadOnlyChatChannel>> ReadOnlyChannelsByDisplayName;

	UPROPERTY(config)
	bool bEnableChatSlashCommands = true;
	
	UPROPERTY()
	TMap<FUniqueNetIdRepl, TObjectPtr<USocialGroupChannel>> GroupChannels;

	mutable FOnChatChannelCreated OnChannelCreatedEvent;
	mutable FOnChatChannelLeft OnChannelLeftEvent;
	mutable FOnChatChannelFocusRequested OnChannelFocusRequestedEvent;
	mutable FOnChatChannelDisplayRequested OnChannelDisplayRequestedEvent;
};

#undef UE_API
