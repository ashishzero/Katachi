#pragma once
#include "Kr/KrCommon.h"

#if PLATFORM_WINDOWS
#include <Winsock2.h>
#include <ws2tcpip.h>
#define poll  WSAPoll
#define errno WSAGetLastError()
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#define SOCKET int
#define INVALID_SOCKET -1
#endif
