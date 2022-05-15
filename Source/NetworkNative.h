#pragma once
#include "Kr/KrCommon.h"

#if PLATFORM_WINDOWS
#include <Winsock2.h>
#include <ws2tcpip.h>
#define poll    WSAPoll
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#define SOCKET int
#define INVALID_SOCKET -1
#endif
