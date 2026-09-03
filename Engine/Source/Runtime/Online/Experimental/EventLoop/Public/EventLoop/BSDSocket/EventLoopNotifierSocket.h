// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BSDSocketTypes.h"
#include "CoreTypes.h"

#if PLATFORM_HAS_BSD_SOCKETS && !PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS

class FEventLoopNotifierSocket
{
public:
	bool Init();
	void Shutdown();

	void Notify();
	void Clear();

	int32 GetFileDescriptorRead() const { return FileDescriptorRead; }

private:
	int32 FileDescriptorListener = INVALID_SOCKET;
	int32 FileDescriptorWrite = INVALID_SOCKET;
	int32 FileDescriptorRead = INVALID_SOCKET;
};

#endif // PLATFORM_HAS_BSD_SOCKETS && !PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS

