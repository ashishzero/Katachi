#include "Network.h"

#if PLATFORM_WINDOWS
#include <Winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#elif PLATFORM_LINUX || PLATFORM_MAC
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define closesocket close
#define SOCKET int
#define INVALID_SOCKET -1
#endif

#ifdef NETWORK_OPENSSL_ENABLE
#include <openssl/ssl.h>
#include <openssl/err.h>
#if PLATFORM_WINDOWS
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "openssl/libcrypto_static.lib")
#pragma comment(lib, "openssl/libssl_static.lib")
#endif
#endif

//
//
//

#if PLATFORM_WINDOWS
static wchar_t *INet_UnicodeToWideChar(Memory_Arena *arena, const char *msg, int length) {
	wchar_t *result = (wchar_t *)PushSize(arena, (length + 1) * sizeof(wchar_t));
	int wlen        = MultiByteToWideChar(CP_UTF8, 0, msg, length, result, length + 1);
	result[wlen]    = 0;
	return result;
}

static Net_Result INet_ConvertNativeError(int error) {
	switch (error) {
		case WSANOTINITIALISED:     return NET_E_INIT;
		case WSATRY_AGAIN:          return NET_E_AGAIN;
		case WSANO_RECOVERY:        return NET_E_FAIL;
		case WSA_NOT_ENOUGH_MEMORY: return NET_E_MEMORY;
		case WSAHOST_NOT_FOUND:     return NET_E_NO_NAME;
		case WSATYPE_NOT_FOUND:     return NET_E_SERVICE;
		case WSAESOCKTNOSUPPORT:    return NET_E_SOCK_TYPE;
		case WSAENETDOWN:           return NET_E_DOWN;
		case WSAEMFILE:             return NET_E_MFILE;
		case WSAENOBUFS:            return NET_E_NO_BUFFER;
		case WSAEAFNOSUPPORT:       return NET_E_UNSUPPORTED;
		case WSAEINVAL:             return NET_E_INVALID_PARAM;
		case WSAEPROTONOSUPPORT:    return NET_E_UNSUPPORTED_PROTO;
		case WSAEPROTOTYPE:         return NET_E_UNSUPPORTED_PROTO;
		case WSAEADDRINUSE:         return NET_E_ADDR_IN_USE;
		case WSAEALREADY:           return NET_E_ALREADY;
		case WSAEADDRNOTAVAIL:      return NET_E_INVALID_ADDR;
		case WSAECONNREFUSED:       return NET_E_CONNECTION_REFUSED;
		case WSAEFAULT:             return NET_E_FAULT;
		case WSAEISCONN:            return NET_E_IS_CONNECTED;
		case WSAENETUNREACH:        return NET_E_NET_UNREACHABLE;
		case WSAEHOSTUNREACH:       return NET_E_HOST_UNREACHABLE;
		case WSAENOTSOCK:           return NET_E_INVALID_PARAM;
		case WSAETIMEDOUT:          return NET_E_TIMED_OUT;
		case WSAEWOULDBLOCK:        return NET_E_WOULD_BLOCK;
		case WSAEACCES:             return NET_E_ACCESS;
		case WSAENETRESET:          return NET_E_RESET;
		case WSAEOPNOTSUPP:         return NET_E_OP_UNSUPPORTED;
		case WSAESHUTDOWN:          return NET_E_SHUTDOWN;
		case WSAEMSGSIZE:           return NET_E_MSG_SIZE;
		case WSAECONNABORTED:       return NET_E_CONNECTION_ABORTED;
		case WSAECONNRESET:         return NET_E_CONNECTION_RESET;
		default:                    return NET_E_SYSTEM;
	}
	return NET_E_SYSTEM;
}

static Net_Result INet_GetLastError() {
	return INet_ConvertNativeError(WSAGetLastError());
}

static bool INet_ShouldReconnect() {
	int error = WSAGetLastError();
	return error == WSAECONNRESET || error == WSAECONNABORTED;
}

static Net_Result INet_OpenSocketDescriptor(const String node, const String service, Net_Socket_Type type, int64_t *pdescriptor) {
	static constexpr int SocketTypeMap[] = { SOCK_STREAM, SOCK_DGRAM };

	*pdescriptor = INVALID_SOCKET;

	ADDRINFOW hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SocketTypeMap[type];

	Memory_Arena *scratch = ThreadScratchpad();
	Temporary_Memory temp = BeginTemporaryMemory(scratch);
	Defer{ EndTemporaryMemory(&temp); };

	wchar_t *nodename = INet_UnicodeToWideChar(scratch, (char *)node.data, (int)node.length);
	wchar_t *servicename = INet_UnicodeToWideChar(scratch, (char *)service.data, (int)service.length);

	ADDRINFOW *address = nullptr;
	int error = GetAddrInfoW(nodename, servicename, &hints, &address);
	if (error) {
		return INet_ConvertNativeError(error);
	}

	SOCKET descriptor = INVALID_SOCKET;
	for (auto ptr = address; ptr; ptr = ptr->ai_next) {
		descriptor = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (descriptor == INVALID_SOCKET) {
			FreeAddrInfoW(address);
			error = WSAGetLastError();
			return INet_ConvertNativeError(error);
		}

		error = connect(descriptor, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (error) {
			closesocket(descriptor);
			descriptor = INVALID_SOCKET;
			continue;
		}

		break;
	}

	FreeAddrInfoW(address);

	*pdescriptor = descriptor;

	if (error)
		return INet_ConvertNativeError(error);

	return NET_OK;
}

#elif PLATFORM_LINUX || PLATFORM_MAC

static char *INet_StringToCString(Memory_Arena *arena, String src) {
	char *dst = (char *)PushSize(arena, src.length + 1);
	memcpy(dst, src.data, src.length);
	dst[src.length] = 0;
	return dst;
}

static Net_Result INet_ConvertNativeError(int error) {
	Unimplemented();
	return NET_E_SYSTEM;
}

static Net_Result INet_GetLastError() {
	return INet_ConvertNativeError(errno);
}

static bool INet_ShouldReconnect() {
	return errno == EPIPE;
}

static Net_Result INet_OpenSocketDescriptor(const String node, const String service, Net_Socket_Type type, int64_t *pdescriptor) {
	static constexpr int SocketTypeMap[] = { SOCK_STREAM, SOCK_DGRAM };

	*pdescriptor = INVALID_SOCKET;

	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SocketTypeMap[type];

	Memory_Arena *scratch = ThreadScratchpad();
	Temporary_Memory temp = BeginTemporaryMemory(scratch);
	Defer{ EndTemporaryMemory(&temp); };

	char *nodename = INet_StringToCString(scratch, node);
	char *servicename = INet_StringToCString(scratch, service);

	addrinfo *address = nullptr;
	int error = getaddrinfo(nodename, servicename, &hints, &address);
	if (error) {
		return INet_ConvertNativeError(error);
	}

	SOCKET descriptor = INVALID_SOCKET;
	for (auto ptr = address; ptr; ptr = ptr->ai_next) {
		descriptor = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (descriptor == INVALID_SOCKET) {
			freeaddrinfo(address);
			return INet_GetLastError();
		}

		error = connect(descriptor, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (error) {
			closesocket(descriptor);
			descriptor = INVALID_SOCKET;
			continue;
		}

		break;
	}

	freeaddrinfo(address);

	*pdescriptor = descriptor;

	if (error)
		return INet_ConvertNativeError(error);

	return NET_OK;
}

#endif

#ifdef NETWORK_OPENSSL_ENABLE
struct Net_Secure_Channel { ptrdiff_t __unused; };

static SSL_CTX *DefaultClientContext;

static uint64_t INet_LogOpenSSLError() {
	char message[1024];
	unsigned long error = ERR_peek_last_error();
	ERR_error_string_n(error, message, sizeof(message));
	WriteLogErrorEx("Net:OpenSSL", "%s", message);
	return error;
}

static bool INet_ShouldReconnectOpenSSL(SSL *ssl, int written) {
	return SSL_get_error(ssl, written) == SSL_ERROR_WANT_CONNECT;
}
#endif

static uint64_t INet_TryReconnecting(Net_Socket *net) {
#ifdef NETWORK_OPENSSL_ENABLE
	bool secure_channel = net->secure_channel ? 1 : 0;
#endif

	Net_CloseConnection(net);
	Net_Result result = INet_OpenSocketDescriptor(net->node, net->service, net->type, &net->descriptor);
	if (result != NET_OK) {
		net->result = result;
		return -1;
	}

#ifdef NETWORK_OPENSSL_ENABLE
	if (secure_channel) {
		uint64_t error = Net_CreateSecureChannel(net);
		if (net->result != NET_OK)
			return error;
	}
#endif

	return 0;
}

inline static SSL *INet_ToSSL(Net_Secure_Channel *channel) { return (SSL *)channel; }
inline static Net_Secure_Channel *INet_ToSecureChannel(SSL *ssl) { return (Net_Secure_Channel *)ssl; }

//
//
//

Net_Result Net_Initialize() {
#if PLATFORM_WINDOWS
	WSADATA wsaData;
	int error = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (error != 0) {
		WriteLogErrorEx("Net:Windows", "WSAStartup failed with error code: %d", error);
		return NET_E_INIT;
	}
#endif

#if PLATFORM_LINUX || PLATFORM_MAC
	signal(SIGPIPE, SIG_IGN);
#endif

#ifdef NETWORK_OPENSSL_ENABLE
	SSL_library_init();
	OpenSSL_add_all_algorithms();

	DefaultClientContext = SSL_CTX_new(TLS_client_method());

	if (!DefaultClientContext) {
		INet_LogOpenSSLError();
		return NET_E_INIT;
	}

	SSL_CTX_set_verify(DefaultClientContext, SSL_VERIFY_PEER, nullptr);

#if PLATFORM_WINDOWS
	X509_STORE *store = SSL_CTX_get_cert_store(DefaultClientContext);
	if (!store) {
		INet_LogOpenSSLError();
		return NET_E_INIT;
	}

	HCERTSTORE cert_store = CertOpenSystemStoreW(0, L"ROOT");
	if (!cert_store) {
		error = GetLastError();
		WriteLogErrorEx("Net:Windows", "CertOpenSystemStoreW failed with error code: %d", error);
		return NET_E_INIT;
	}
	Defer{ CertCloseStore(cert_store, 0); };

	PCCERT_CONTEXT cert_context = nullptr;
	while (cert_context = CertEnumCertificatesInStore(cert_store, cert_context)) {
		X509 *x509 = d2i_X509(nullptr, (const unsigned char **)&cert_context->pbCertEncoded, cert_context->cbCertEncoded);

		if (x509) {
			int i = X509_STORE_add_cert(store, x509);
			X509_free(x509);
		}
	}
	CertFreeCertificateContext(cert_context);
#endif

#if PLATFORM_LINUX || PLATFORM_MAC
	long res = SSL_CTX_set_default_verify_paths(DefaultClientContext);

	if (res != 1) {
		INet_LogOpenSSLError();
		return NET_E_INIT;
	}
#endif
#endif

	return NET_OK;
}

void Net_Shutdown() {
#ifdef NETWORK_OPENSSL_ENABLE
	SSL_CTX_free(DefaultClientContext);
#endif

#if PLATFORM_WINDOWS
	WSACleanup();
#endif
}

//
//
//

Net_Result Net_GetLastError(Net_Socket *net) {
	return net->result;
}

void Net_ClearError(Net_Socket *net) {
	net->result = NET_OK;
}

Net_Socket Net_OpenConnection(const String node, const String service, Net_Socket_Type type, Memory_Allocator allocator) {
	Net_Socket net;
	memset(&net, 0, sizeof(net));

	Net_Result result = INet_OpenSocketDescriptor(node, service, type, &net.descriptor);
	if (result != NET_OK) {
		net.result = result;
		return net;
	}

	net.type      = type;
	net.allocator = allocator;

	uint8_t *mem = (uint8_t *)MemoryAllocate(node.length + service.length + 2, net.allocator);

	if (mem) {
		net.node.data = mem;
		net.service.data = mem + node.length + 1;
		net.node.length = node.length;
		net.service.length = service.length;

		memcpy(net.node.data, node.data, node.length);
		memcpy(net.service.data, service.data, service.length);
		net.node.data[node.length] = 0;
		net.service.data[service.length] = 0;
	} else {
		closesocket((SOCKET)net.descriptor);
		net.descriptor = INVALID_SOCKET;
		net.result     = NET_E_MEMORY;
	}

	return net;
}

void Net_CloseConnection(Net_Socket *net) {
#ifdef NETWORK_OPENSSL_ENABLE
	if (net->secure_channel) {
		SSL *ssl = INet_ToSSL(net->secure_channel);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		net->secure_channel = nullptr;
	}
#endif

	closesocket((SOCKET)net->descriptor);
	net->descriptor = INVALID_SOCKET;

	MemoryFree(net->node.data, net->node.length + net->service.length + 2, net->allocator);
	net->node    = {};
	net->service = {};
}

int Net_Write(Net_Socket *net, void *buffer, int length) {
	int written = send((SOCKET)net->descriptor, (char *)buffer, length, 0);

	if (written > 0) {
		net->result = NET_OK;
		return written;
	}

	if (INet_ShouldReconnect()) {
		INet_TryReconnecting(net);
		if (net->result == NET_OK) {
			written = send((SOCKET)net->descriptor, (char *)buffer, length, 0);
			if (written > 0)
				net->result = NET_OK;
			else
				net->result = INet_GetLastError();
			return written;
		}
		return written;
	}

	net->result = INet_GetLastError();
	return written;
}

int Net_Read(Net_Socket *net, void *buffer, int length) {
	int read = recv((SOCKET)net->descriptor, (char *)buffer, length, 0);
	if (read > 0)
		net->result = NET_OK;
	else
		net->result= INet_GetLastError();
	return read;
}

#ifdef NETWORK_OPENSSL_ENABLE
uint64_t Net_CreateSecureChannel(Net_Socket *net) {
	SSL *ssl = SSL_new(DefaultClientContext);

	if (!ssl) {
		net->result = NET_E_OPENSSL;
		return INet_LogOpenSSLError();
	}

	if (!SSL_set_tlsext_host_name(ssl, (char *)net->node.data)) {
		SSL_free(ssl);
		net->result = NET_E_OPENSSL;
		return INet_LogOpenSSLError();
	}

	SSL_set_fd(ssl, (int)net->descriptor);
	if (SSL_connect(ssl) == -1) {
		SSL_free(ssl);
		net->result = NET_E_OPENSSL;
		return INet_LogOpenSSLError();
	}

	net->secure_channel = INet_ToSecureChannel(ssl);

	return NET_OK;
}

int Net_WriteSecured(Net_Socket *net, void *buffer, int length) {
	SSL *ssl = INet_ToSSL(net->secure_channel);
	int written = SSL_write(ssl, (char *)buffer, length);

	if (written > 0) {
		net->result = NET_OK;
		return written;
	}

	if (INet_ShouldReconnectOpenSSL(ssl, written)) {
		written = SSL_write(ssl, (char *)buffer, length);
		if (written > 0) {
			net->result = NET_OK;
			return written;
		}
	}

	INet_LogOpenSSLError();
	net->result = NET_E_OPENSSL;
	return written;
}

int Net_ReadSecured(Net_Socket *net, void *buffer, int length) {
	SSL *ssl = INet_ToSSL(net->secure_channel);
	int read = SSL_read(ssl, buffer, length);
	if (read > 0) {
		net->result = NET_OK;
		return read;
	}
	INet_LogOpenSSLError();
	net->result = NET_E_OPENSSL;
	return read;
}

#endif
