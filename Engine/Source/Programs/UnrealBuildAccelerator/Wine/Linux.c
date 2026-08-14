// Copyright Epic Games, Inc. All Rights Reserved.

#include <netinet/tcp.h>
#include <netinet/in.h>
#include <errno.h>

int unix_get_tcp_info(int fd, void *buf, int *len)
{
	socklen_t l = *len;
	if (getsockopt(fd, IPPROTO_TCP, TCP_INFO, buf, &l) == -1)
		return -errno;

	*len = (int)l;
	return 0;
}
