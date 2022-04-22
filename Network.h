#include "Kr/KrCommon.h"

enum Net_Socket_Type {
	NET_SOCKET_TCP,
	NET_SOCKET_UDP
};

enum Net_Result {
	NET_OK,
	NET_E_OPENSSL,
	NET_E_INIT,
	NET_E_ADDR_FAMILY,
	NET_E_AGAIN,
	NET_E_FAIL,
	NET_E_MEMORY,
	NET_E_NO_DATA,
	NET_E_NO_NAME,
	NET_E_SERVICE,
	NET_E_SOCK_TYPE,
	NET_E_DOWN,
	NET_E_MFILE,
	NET_E_NO_BUFFER,
	NET_E_UNSUPPORTED_PROTO,
	NET_E_INVALID_PARAM,
	NET_E_UNSUPPORTED,
	NET_E_SYSTEM,
	NET_E_ADDR_IN_USE,
	NET_E_ALREADY,
	NET_E_INVALID_ADDR,
	NET_E_CONNECTION_REFUSED,
	NET_E_FAULT,
	NET_E_IS_CONNECTED,
	NET_E_NET_UNREACHABLE,
	NET_E_HOST_UNREACHABLE,
	NET_E_TIMED_OUT,
	NET_E_WOULD_BLOCK,
	NET_E_ACCESS,
	NET_E_RESET,
	NET_E_OP_UNSUPPORTED,
	NET_E_SHUTDOWN,
	NET_E_MSG_SIZE,
	NET_E_CONNECTION_ABORTED,
	NET_E_CONNECTION_RESET,

	_NET_RESULT_COUNT
};

struct Net_Secure_Channel;

struct Net_Socket {
#ifdef NETWORK_OPENSSL_ENABLE
	Net_Secure_Channel *secure_channel;
#endif
	int64_t            descriptor;
	Net_Socket_Type    type;
	String             node;
	String             service;
	Net_Result         result;
	Memory_Allocator   allocator;
};

Net_Result   Net_Initialize();
void         Net_Shutdown();

Net_Result   Net_GetLastError(Net_Socket *net);
void         Net_ClearError(Net_Socket *net);

Net_Socket   Net_OpenConnection(const String node, const String service, Net_Socket_Type type, Memory_Allocator allocator = ThreadContext.allocator);
void         Net_CloseConnection(Net_Socket *net);

int          Net_Write(Net_Socket *net, void *buffer, int length);
int          Net_Read(Net_Socket *net, void *buffer, int length);

#ifdef NETWORK_OPENSSL_ENABLE
uint64_t     Net_CreateSecureChannel(Net_Socket *net);
int          Net_WriteSecured(Net_Socket *net, void *buffer, int length);
int          Net_ReadSecured(Net_Socket *net, void *buffer, int length);
#endif
