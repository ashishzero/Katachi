#pragma once
#include "Kr/KrCommon.h"

constexpr int NET_TIMEOUT_MILLISECS = 2000;

enum Net_Error {
	NET_E_NONE,
	NET_E_TIMED_OUT = 1,
	NET_E_WOULD_BLOCK,
	NET_E_CONNECTION_LOST,
	NET_E_OUT_OF_MEMORY
};

constexpr int NET_MAX_CANON_NAME = 2048;

enum Net_Socket_Type {
	NET_SOCKET_TCP,
	NET_SOCKET_UDP
};

struct Net_Socket;

bool   Net_Initialize();
void   Net_Shutdown();

/*
* Send: -ve means error, +ve means number of bytes sent, 0 means success or wait
* Receive: -ve means error, +ve means number of bytes received, 0 means wait
*/

Net_Socket * Net_OpenConnection(const String node, const String service, Net_Socket_Type type, ptrdiff_t user_size, Memory_Allocator allocator = ThreadContext.allocator);
Net_Socket  *Net_OpenConnection(const String node, const String service, Net_Socket_Type type, Memory_Allocator allocator = ThreadContext.allocator);
bool         Net_OpenSecureChannel(Net_Socket *net, bool verify = true);
void         Net_CloseConnection(Net_Socket *net);
void         Net_Shutdown(Net_Socket *net);
void         Net_SetSocketReceiveBufferSize(Net_Socket *net, int size);
void         Net_SetSocketSendBufferSize(Net_Socket *net, int size);
void *       Net_GetUserBuffer(Net_Socket *net);
Net_Error    Net_GetLastError(Net_Socket *net);
void         Net_SetError(Net_Socket *net, Net_Error error);
bool         Net_TryReconnect(Net_Socket *net);
String       Net_GetHostname(Net_Socket *net);
int          Net_GetPort(Net_Socket *net);
int32_t      Net_GetSocketDescriptor(Net_Socket *net);
bool         Net_SetSocketBlockingMode(Net_Socket *net, bool blocking);
int          Net_SendBlocked(Net_Socket *net, void *buffer, int length, int timeout = NET_TIMEOUT_MILLISECS);
int          Net_ReceiveBlocked(Net_Socket *net, void *buffer, int length, int timeout = NET_TIMEOUT_MILLISECS);
int          Net_Send(Net_Socket *net, void *buffer, int length);
int          Net_Receive(Net_Socket *net, void *buffer, int length);
