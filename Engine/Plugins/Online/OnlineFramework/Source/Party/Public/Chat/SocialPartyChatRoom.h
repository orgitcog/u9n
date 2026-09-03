// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialChatRoom.h"
#include "SocialPartyChatRoom.generated.h"

#define UE_API PARTY_API

class UPartyMember;
enum class ESocialChannelType : uint8;

class USocialChatManager;
enum class EMemberExitedReason : uint8;

/** A multi-user chat room channel. Used for all chat situations outside of private user-to-user direct messages. */
UCLASS(MinimalAPI)
class USocialPartyChatRoom : public USocialChatRoom
{
	GENERATED_BODY()

public:
	UE_API virtual void Initialize(USocialUser* InSocialUser, const FChatRoomId& InChannelId, ESocialChannelType InSourceChannelType);

protected:
	UE_API virtual void HandlePartyMemberLeft(EMemberExitedReason Reason);
	UE_API virtual void HandlePartyMemberJoined(UPartyMember& NewMember);
};

#undef UE_API
