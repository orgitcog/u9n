// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPAddress.h"

#if WITH_EOS_SDK
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_common.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogSocketSubsystemEOS, Log, All);

#define EOS_SOCKET_NAME_SIZE 33

class FInternetAddrEOS
	: public FInternetAddr
{
public:
	SOCKETSUBSYSTEMEOS_API FInternetAddrEOS();
	SOCKETSUBSYSTEMEOS_API FInternetAddrEOS(const FString& InRemoteUserId, const FString& InSocketName, const int32 InChannel);
#if WITH_EOS_SDK
	SOCKETSUBSYSTEMEOS_API FInternetAddrEOS(const EOS_ProductUserId InRemoteUserId, const FString& InSocketName, const int32 InChannel);
#endif
	virtual ~FInternetAddrEOS() = default;

//~ Begin FInternetAddr Interface
	SOCKETSUBSYSTEMEOS_API virtual void SetIp(uint32 InAddr) override;
	SOCKETSUBSYSTEMEOS_API virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override;
	SOCKETSUBSYSTEMEOS_API virtual void GetIp(uint32& OutAddr) const override;
	SOCKETSUBSYSTEMEOS_API virtual void SetPort(int32 InPort) override;
	SOCKETSUBSYSTEMEOS_API virtual int32 GetPort() const override;
	SOCKETSUBSYSTEMEOS_API virtual void SetRawIp(const TArray<uint8>& RawAddr) override;
	SOCKETSUBSYSTEMEOS_API virtual TArray<uint8> GetRawIp() const override;
	SOCKETSUBSYSTEMEOS_API virtual void SetAnyAddress() override;
	SOCKETSUBSYSTEMEOS_API virtual void SetBroadcastAddress() override;
	SOCKETSUBSYSTEMEOS_API virtual void SetLoopbackAddress() override;
	SOCKETSUBSYSTEMEOS_API virtual FString ToString(bool bAppendPort) const override;
	SOCKETSUBSYSTEMEOS_API virtual uint32 GetTypeHash() const override;
	SOCKETSUBSYSTEMEOS_API virtual bool IsValid() const override;
	SOCKETSUBSYSTEMEOS_API virtual TSharedRef<FInternetAddr> Clone() const override;
//~ End FInternetAddr Interface

	FORCEINLINE FInternetAddrEOS& operator=(const FInternetAddrEOS& Other)
	{
		LocalUserId = Other.LocalUserId;
		RemoteUserId = Other.RemoteUserId;
		FCStringAnsi::Strcpy(SocketName, Other.SocketName);
		Channel = Other.Channel;
		return *this;
	}
	
	FORCEINLINE friend bool operator==(const FInternetAddrEOS& A, const FInternetAddrEOS& B)
	{
		return A.Channel == B.Channel
#if WITH_EOS_SDK
			&& A.LocalUserId == B.LocalUserId
			&& A.RemoteUserId == B.RemoteUserId
#endif
			&& FCStringAnsi::Stricmp(A.GetSocketName(), B.GetSocketName()) == 0;
	}

	FORCEINLINE friend bool operator!=(const FInternetAddrEOS& A, const FInternetAddrEOS& B)
	{
		return !(A == B);
	}

	FORCEINLINE friend uint32 GetTypeHash(const FInternetAddrEOS& Address)
	{
		return Address.GetTypeHash();
	}

	friend bool operator<(const FInternetAddrEOS& Left, const FInternetAddrEOS& Right)
	{
#if WITH_EOS_SDK
		if (Left.GetLocalUserId() < Right.GetLocalUserId())
		{
			return true;
		}
		else if (Left.GetRemoteUserId() < Right.GetRemoteUserId())
		{
			return true;
		}
		else
#endif
		if (FCStringAnsi::Stricmp(Left.GetSocketName(), Right.GetSocketName()) < 0)
		{
			return true;
		}

		return Left.GetChannel() < Right.GetChannel();
	}

#if WITH_EOS_SDK
	EOS_ProductUserId GetLocalUserId() const
	{
		return LocalUserId;
	}

	void SetLocalUserId(EOS_ProductUserId InLocalUserId)
	{
		LocalUserId = InLocalUserId;
	}

	void SetRemoteUserId(EOS_ProductUserId InRemoteUserId)
	{
		RemoteUserId = InRemoteUserId;
	}

	EOS_ProductUserId GetRemoteUserId() const
	{
		return RemoteUserId;
	}
#else
	void* GetLocalUserId() const
	{
		return LocalUserId;
	}

	void SetLocalUserId(void* InLocalUserId)
	{
		LocalUserId = InLocalUserId;
	}

	void SetRemoteUserId(void* InRemoteUserId)
	{
		RemoteUserId = InRemoteUserId;
	}

	void* GetRemoteUserId() const
	{
		return RemoteUserId;
	}
#endif
	
	const char* GetSocketName() const
	{
		return SocketName;
	}

	void SetSocketName(const FString& InSocketName)
	{
		FCStringAnsi::Strncpy(SocketName, TCHAR_TO_UTF8(*InSocketName), EOS_SOCKET_NAME_SIZE);
	}

	void SetSocketName(const char* InSocketName)
	{
		FCStringAnsi::Strncpy(SocketName, InSocketName, EOS_SOCKET_NAME_SIZE);
	}

	uint8 GetChannel() const
	{
		return Channel;
	}

	void SetChannel(uint8 InChannel)
	{
		Channel = InChannel;
	}

private:
#if WITH_EOS_SDK
	EOS_ProductUserId LocalUserId;
	EOS_ProductUserId RemoteUserId;
#else
	void* LocalUserId;
	void* RemoteUserId;
#endif
	char SocketName[EOS_SOCKET_NAME_SIZE];
	uint8 Channel;

	friend class SocketSubsystemEOS;
};
