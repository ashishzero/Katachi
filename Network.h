#include "Kr/KrCommon.h"

enum Net_Socket_Type {
	NET_SOCKET_STREAM,
	NET_SOCKET_DGRAM
};

struct Net_Secure_Channel;

struct Net_Socket {
#ifdef NETWORK_OPENSSL_ENABLE
	Net_Secure_Channel *secure_channel;
#endif
	int64_t            descriptor;
	uint32_t           flags;
	Net_Socket_Type    type;
	String             node;
	String             service;
	Memory_Allocator   allocator;
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

Net_Result   Net_Initialize();
void         Net_Shutdown();

Net_Result   Net_OpenConnection(const String node, const String service, Net_Socket_Type type, Net_Socket *net, Memory_Allocator allocator = ThreadContext.allocator);
void         Net_CloseConnection(Net_Socket *net);

Net_Result   Net_Write(Net_Socket *net, void *buffer, int length, int *written);
Net_Result   Net_Read(Net_Socket *net, void *buffer, int length, int *read);

#ifdef NETWORK_OPENSSL_ENABLE
Net_Result   Net_CreateSecureChannel(Net_Socket *net);
Net_Result   Net_VerifyRemoteCertificate(Net_Socket *net);
Net_Result   Net_WriteSecured(Net_Socket *net, void *buffer, int length, int *written);
Net_Result   Net_ReadSecured(Net_Socket *net, void *buffer, int length, int *read);
#endif
