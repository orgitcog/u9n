// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#if HAS_EVENTLOOP_NOTIFIER_EVENTFD
#include "EventLoop/BSDSocket/EventLoopNotifierEventFd.h"
using FEventLoopNotifier = FEventLoopNotifierEventFd;
#elif PLATFORM_HAS_BSD_SOCKETS && !PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
#include "EventLoop/BSDSocket/EventLoopNotifierSocket.h"
using FEventLoopNotifier = FEventLoopNotifierSocket;
#endif
