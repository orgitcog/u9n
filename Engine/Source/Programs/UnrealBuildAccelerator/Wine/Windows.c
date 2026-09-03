// Copyright Epic Games, Inc. All Rights Reserved.

#include <winsock2.h>
#include <stdint.h>
#include <unistd.h>

NTSYSAPI NTSTATUS CDECL wine_server_handle_to_fd( HANDLE handle, unsigned int access, int *unix_fd, unsigned int *options );

extern int unix_get_tcp_info(int fd, void *buf, int *len);

INT WINAPI GetLinuxTcpInfo(SOCKET s, void *buf, INT *len)
{
	int fd = -1;
	if (wine_server_handle_to_fd((HANDLE)s, 0, &fd, NULL) != 0)
		return -1;
	int res = unix_get_tcp_info(fd, buf, len);
	close(fd);
	return res;
}
