// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventLoop/BSDSocket/EventLoopNotifierEventFd.h"

#if HAS_EVENTLOOP_NOTIFIER_EVENTFD
#include "BSDSocketTypesPrivate.h"

bool FEventLoopNotifierEventFd::Init()
{
	FileDescriptorRead = eventfd(0, EFD_NONBLOCK);
	return FileDescriptorRead != INVALID_SOCKET;
}

void FEventLoopNotifierEventFd::Shutdown()
{
	if (FileDescriptorRead != INVALID_SOCKET)
	{
		close(FileDescriptorRead);
		FileDescriptorRead = INVALID_SOCKET;
	}
}

void FEventLoopNotifierEventFd::Notify()
{
	eventfd_t Value = 1;
	if (eventfd_write(FileDescriptorRead, Value) == SOCKET_RESULT_FAILED)
	{
		int Err = errno;
		check(Err == EAGAIN);
	}
}

void FEventLoopNotifierEventFd::Clear()
{
	eventfd_t Value = 0;
	ensure(eventfd_read(FileDescriptorRead, &Value) != SOCKET_RESULT_FAILED);
}

#endif // HAS_EVENTLOOP_NOTIFIER_EVENTFD

