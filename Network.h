#pragma once
#include "Kr/KrCommon.h"

constexpr int NET_TIMEOUT_SECS = 1;

enum Net_Socket_Type {
	NET_SOCKET_TCP,
	NET_SOCKET_UDP
};

struct Net_Socket;

bool   Net_Initialize();
void   Net_Shutdown();

Net_Socket  *Net_OpenConnection(const String node, const String service, Net_Socket_Type type, Memory_Allocator allocator = ThreadContext.allocator);
bool         Net_OpenSecureChannel(Net_Socket *net, bool verify = true);
void         Net_CloseConnection(Net_Socket *net);
String       Net_GetHostname(Net_Socket *net);
ptrdiff_t    Net_GetSocketDescriptor(Net_Socket *net);
int          Net_Write(Net_Socket *net, void *buffer, int length);
int          Net_Read(Net_Socket *net, void *buffer, int length);
