#include "Network.h"

#include "NetworkNative.h"

#if PLATFORM_WINDOWS
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <stdio.h>
#endif

#ifdef NETWORK_OPENSSL_ENABLE
#include <openssl/ssl.h>
#include <openssl/err.h>
#if PLATFORM_WINDOWS
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "openssl/libcrypto_static.lib")
#pragma comment(lib, "openssl/libssl_static.lib")
#endif
#endif

//
//
//

constexpr int NET_DEFAULT_USER_SIZE = 8;

typedef int(*Net_Write_Proc)(struct Net_Socket *net, void *buffer, int length);
typedef int(*Net_Read_Proc)(struct Net_Socket *net, void *buffer, int length);

struct Net_Socket {
	Net_Write_Proc   write;
	Net_Read_Proc    read;
#ifdef NETWORK_OPENSSL_ENABLE
	SSL *            ssl;
#endif
	SOCKET           descriptor;
	Net_Error        error;
	int              family;
	int              type;
	int              protocol;
	int              hostlen;
	char             hostname[NET_MAX_CANON_NAME];
	int              addrlen;
	sockaddr_storage address;
	Memory_Allocator allocator;
	ptrdiff_t        allocated;
	uint8_t          user[NET_DEFAULT_USER_SIZE];
};

static constexpr int SocketTypeMap[] = { SOCK_STREAM, SOCK_DGRAM };

//
//
//

#if PLATFORM_WINDOWS
static void PL_Net_UnicodeToWideChar(wchar_t *dst, int dst_len, const char *msg, int length) {
	int wlen = MultiByteToWideChar(CP_UTF8, 0, msg, length, dst, dst_len - 1);
	dst[wlen] = 0;
}

static void PL_Net_ReportError(int error) {
	DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
	wchar_t *msg = NULL;
	FormatMessageW(flags, NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msg, 0, NULL);
	LogErrorEx("Net:Windows", "%S", msg);
	LocalFree(msg);
}
#define PL_Net_ReportLastPlatformError() PL_Net_ReportError(GetLastError())
#define PL_Net_ReportLastSocketError() PL_Net_ReportError(WSAGetLastError())

static void PL_Net_Shutdown() {
	WSACleanup();
}

static bool PL_Net_Initialize() {
	WSADATA wsaData;
	int error = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (error != 0) {
		PL_Net_ReportError(error);
		return false;
	}

	timeBeginPeriod(1);

	return true;
}

static SOCKET PL_Net_OpenSocketDescriptor(const String node, const String service, Net_Socket_Type type, char(&hostname)[NET_MAX_CANON_NAME], sockaddr_storage *addr, ptrdiff_t *addrelen, int *pfamily, int *ptype, int *pprotocol) {
	ADDRINFOW hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags    = AI_CANONNAME;
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SocketTypeMap[type];

	wchar_t nodename[2048];
	wchar_t servicename[512];

	if (node.length + 1 >= ArrayCount(nodename) || service.length + 1 >= ArrayCount(servicename)) {
		LogError("Net:Windows", "Could not create socket: Out of memory");
		return INVALID_SOCKET;
	}

	PL_Net_UnicodeToWideChar(nodename, ArrayCount(nodename), (char *)node.data, (int)node.length);
	PL_Net_UnicodeToWideChar(servicename, ArrayCount(servicename), (char *)service.data, (int)service.length);

	ADDRINFOW *address = nullptr;
	int error = GetAddrInfoW(nodename, servicename, &hints, &address);
	if (error) {
		PL_Net_ReportError(error);
		return INVALID_SOCKET;
	}

	SOCKET descriptor = INVALID_SOCKET;
	for (auto ptr = address; ptr; ptr = ptr->ai_next) {
		descriptor = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (descriptor == INVALID_SOCKET) {
			FreeAddrInfoW(address);
			PL_Net_ReportLastSocketError();
			return INVALID_SOCKET;
		}

		error = connect(descriptor, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (error) {
			closesocket(descriptor);
			descriptor = INVALID_SOCKET;
			continue;
		}

		snprintf(hostname, sizeof(hostname), "%S", address->ai_canonname);
		*addrelen = ptr->ai_addrlen;
		memcpy(addr, ptr->ai_addr, ptr->ai_addrlen);

		*pfamily   = ptr->ai_family;
		*ptype     = ptr->ai_socktype;
		*pprotocol = ptr->ai_protocol;

		break;
	}

	FreeAddrInfoW(address);

	if (error) {
		PL_Net_ReportError(WSAGetLastError());
	}

	return descriptor;
}

static void PL_Net_CloseSocketDescriptor(SOCKET descriptor) {
	closesocket(descriptor);
}

#elif PLATFORM_LINUX || PLATFORM_MAC

static void PL_Net_ReportError(int error) {
	const char *source = "";
	#if PLATFORM_LINUX
	source = "Net:Linux";
	#elif PLATFORM_MAC
	source = "Net:Mac";
	#endif
	const char *msg = "";
	if (error == EAI_SYSTEM)
		msg = strerror(errno);
	else
		msg = gai_strerror(error);
	LogErrorEx(source, "%s", msg);
}
#define PL_Net_ReportLastPlatformError(EAI_SYSTEM)
#define PL_Net_ReportLastSocketError(EAI_SYSTEM)

static void PL_Net_Shutdown() {}

static void SignalHandler(int signo) {
    switch (signo) {
        case SIGPIPE:
            break;
        default:
            // unreachable
            break;
    }
}

static bool PL_Net_Initialize() {
	signal(SIGPIPE, SignalHandler);
	return true;
}

static SOCKET PL_Net_OpenSocketDescriptor(const String node, const String service, Net_Socket_Type type, char(&hostname)[NET_MAX_CANON_NAME], sockaddr_storage *addr, ptrdiff_t *addrelen, int *pfamily, int *ptype, int *pprotocol) {
	static constexpr int SocketTypeMap[] = { SOCK_STREAM, SOCK_DGRAM };

	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags    = AI_CANONNAME;
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SocketTypeMap[type];

	char nodename[2048];
	char servicename[512];

	if (node.length + 1 >= ArrayCount(nodename) || service.length + 1 >= ArrayCount(servicename)) {
		const char *source = "";
#if PLATFORM_LINUX
			source = "Net:Linux";
#elif PLATFORM_MAC
			source = "Net:Mac";
#endif
		LogErrorEx(source, "Could not create socket: Out of memory");
		return INVALID_SOCKET;
	}

	memcpy(nodename, node.data, node.length);
	memcpy(servicename, service.data, service.length);

	nodename[node.length]       = 0;
	servicename[service.length] = 0;

	addrinfo *address = nullptr;
	int error = getaddrinfo(nodename, servicename, &hints, &address);
	if (error) {
		PL_Net_ReportError(error);
		return INVALID_SOCKET;
	}

	SOCKET descriptor = INVALID_SOCKET;
	for (auto ptr = address; ptr; ptr = ptr->ai_next) {
		descriptor = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (descriptor == INVALID_SOCKET) {
			freeaddrinfo(address);
			PL_Net_ReportError(error);
			return INVALID_SOCKET;
		}

		error = connect(descriptor, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (error) {
			close(descriptor);
			descriptor = INVALID_SOCKET;
			continue;
		}

		snprintf(hostname, sizeof(hostname), "%s", address->ai_canonname);
		*addrelen = ptr->ai_addrlen;
		memcpy(addr, ptr->ai_addr, ptr->ai_addrlen);

		*pfamily   = ptr->ai_family;
		*ptype     = ptr->ai_socktype;
		*pprotocol = ptr->ai_protocol;

		break;
	}

	freeaddrinfo(address);

	if (error) {
		PL_Net_ReportError(errno);
	}

	return descriptor;
}

static void PL_Net_CloseSocketDescriptor(SOCKET descriptor) {
	close(descriptor);
}
#endif

//
//
//

static int PL_Net_Write(Net_Socket *net, void *buffer, int length) {
	int written = send((SOCKET)net->descriptor, (char *)buffer, length, 0);
	return written;
}

static int PL_Net_Read(Net_Socket *net, void *buffer, int length) {
	int read = recv((SOCKET)net->descriptor, (char *)buffer, length, 0);
	return read;
}

//
//
//

#ifdef NETWORK_OPENSSL_ENABLE
static SSL_CTX *DefaultClientContext;
static SSL_CTX *DefaultClientVerifyContext;

static void PL_Net_ReportOpenSSLError() {
	char message[1024];
	unsigned long error;

	while (1) {
		error = ERR_get_error();
		if (!error) return;
		ERR_error_string_n(error, message, sizeof(message));
		LogErrorEx("Net:OpenSSL", "%s", message);
	}
}

static void PL_Net_OpenSSLShutdown() {
	SSL_CTX_free(DefaultClientContext);
	SSL_CTX_free(DefaultClientVerifyContext);
}

static bool PL_Net_OpenSSLInitialize() {
	SSL_library_init();
	OpenSSL_add_all_algorithms();

	DefaultClientContext = SSL_CTX_new(TLS_client_method());
	DefaultClientVerifyContext = SSL_CTX_new(TLS_client_method());

	if (!DefaultClientContext || !DefaultClientVerifyContext) {
		PL_Net_ReportOpenSSLError();

		if (DefaultClientContext)
			SSL_CTX_free(DefaultClientContext);
		if (DefaultClientVerifyContext)
			SSL_CTX_free(DefaultClientVerifyContext);

		return false;
	}

	SSL_CTX_set_verify(DefaultClientVerifyContext, SSL_VERIFY_PEER, nullptr);

#if PLATFORM_WINDOWS
	X509_STORE *store = SSL_CTX_get_cert_store(DefaultClientVerifyContext);
	if (!store) {
		PL_Net_ReportOpenSSLError();
		PL_Net_OpenSSLShutdown();
		return false;
	}

	HCERTSTORE cert_store = CertOpenSystemStoreW(0, L"ROOT");
	if (!cert_store) {
		PL_Net_ReportLastPlatformError();
		PL_Net_OpenSSLShutdown();
		return false;
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
	long res = SSL_CTX_set_default_verify_paths(DefaultClientVerifyContext);

	if (res != 1) {
		PL_Net_ReportOpenSSLError();
		PL_Net_OpenSSLShutdown();
		return false;
	}
#endif

	return true;
}

static int PL_Net_OpenSSLWrite(Net_Socket *net, void *buffer, int length) {
	int written = SSL_write(net->ssl, buffer, length);
	return written;
}

static int PL_Net_OpenSSLRead(Net_Socket *net, void *buffer, int length) {
	int read = SSL_read(net->ssl, buffer, length);
	return read;
}

static bool PL_Net_OpenSSLOpenChannel(Net_Socket *net, bool verify) {
	SSL *ssl = SSL_new(verify ? DefaultClientVerifyContext : DefaultClientContext);

	if (!ssl) {
		PL_Net_ReportOpenSSLError();
		return false;
	}

	if (!SSL_set_tlsext_host_name(ssl, net->hostname)) {
		PL_Net_ReportOpenSSLError();
		SSL_free(ssl);
		return false;
	}

	SSL_set_fd(ssl, (int)net->descriptor);
	if (SSL_connect(ssl) == -1) {
		PL_Net_ReportOpenSSLError();
		SSL_free(ssl);
		return false;
	}

	net->ssl   = ssl;
	net->read  = PL_Net_OpenSSLRead;
	net->write = PL_Net_OpenSSLWrite;

	return true;
}

static void PL_Net_OpenSSLCloseChannel(Net_Socket *net) {
	if (net->ssl) {
		SSL_shutdown(net->ssl);
		SSL_free(net->ssl);
	}
}

static bool PL_Net_OpenSSLReconnect(Net_Socket *net) {
	if (net->ssl) {
		if (SSL_connect(net->ssl) == -1) {
			PL_Net_ReportOpenSSLError();
			return false;
		}
	}
	return true;
}
#else
#define PL_Net_OpenSSLInitialize(...) (true)
#define PL_Net_OpenSSLShutdown(...)
#define PL_Net_OpenSSLOpenChannel(...) (false)
#define PL_Net_OpenSSLCloseChannel(...)
#define PL_Net_OpenSSLResetDescriptor(...) (true)
#define PL_Net_OpenSSLReconnect(...) (true)
#endif

//
//
//

static bool IsInitialized = false;

bool Net_Initialize() {
	if (IsInitialized) return true;

	if (!PL_Net_Initialize())
		return false;
	IsInitialized = PL_Net_OpenSSLInitialize();
	return IsInitialized;
}

void Net_Shutdown() {
	if (IsInitialized) {
		PL_Net_OpenSSLShutdown();
		PL_Net_Shutdown();
	}
}

//
//
//

Net_Socket *Net_OpenConnection(const String node, const String service, Net_Socket_Type type, ptrdiff_t user_size, Memory_Allocator allocator) {
	char hostname[NET_MAX_CANON_NAME];

	sockaddr_storage addr;
	ptrdiff_t        addr_len;
	int              family, socktype, protocol;
	SOCKET descriptor = PL_Net_OpenSocketDescriptor(node, service, type, hostname, &addr, &addr_len, &family, &socktype, &protocol);
	if (descriptor == INVALID_SOCKET)
		return nullptr;

	user_size = Maximum(user_size, NET_DEFAULT_USER_SIZE);
	ptrdiff_t allocation_size = user_size - NET_DEFAULT_USER_SIZE;
	allocation_size += sizeof(Net_Socket);

	Net_Socket *net = (Net_Socket *)MemoryAllocate(allocation_size, allocator);

	if (net) {
		memset(net, 0, sizeof(*net));

		net->write      = PL_Net_Write;
		net->read       = PL_Net_Read;
		net->descriptor = descriptor;
		net->family     = family;
		net->type       = socktype;
		net->protocol   = protocol;
		net->allocator  = allocator;
		net->addrlen    = (int)addr_len;
		net->hostlen    = (int)strlen(hostname);
		net->allocated  = allocation_size;

		memcpy(net->hostname, hostname, sizeof(hostname));
		memcpy(&net->address, &addr, sizeof(addr));

		memset(net->user, 0, user_size);

		return net;
	}

	LogErrorEx("Net", "Failed to allocate memory for socket");

	PL_Net_CloseSocketDescriptor(descriptor);

	return nullptr;
}

Net_Socket *Net_OpenConnection(const String node, const String service, Net_Socket_Type type, Memory_Allocator allocator) {
	return Net_OpenConnection(node, service, type, NET_DEFAULT_USER_SIZE, allocator);
}

bool Net_OpenSecureChannel(Net_Socket *net, bool verify) {
	return PL_Net_OpenSSLOpenChannel(net, verify);
}

void Net_CloseConnection(Net_Socket *net) {
	PL_Net_OpenSSLCloseChannel(net);
	PL_Net_CloseSocketDescriptor(net->descriptor);
	MemoryFree(net, net->allocated, net->allocator);
}

void Net_Shutdown(Net_Socket *net) {
#if PLATFORM_WINDOWS
	int how = SD_BOTH;
#else
	int how = SHUT_RDWR;
#endif
	shutdown(net->descriptor, how);
}

void *Net_GetUserBuffer(Net_Socket *net) {
	return net->user;
}

Net_Error Net_GetLastError(Net_Socket *net) {
	return net->error;
}

void Net_SetError(Net_Socket *net, Net_Error error) {
	net->error = error;
}

bool Net_TryReconnect(Net_Socket *net) {
	PL_Net_CloseSocketDescriptor(net->descriptor);

	net->descriptor = socket(net->family, net->type, net->protocol);
	if (net->descriptor < 0) {
		net->descriptor = INVALID_SOCKET;
		PL_Net_ReportLastSocketError();
		return false;
	}

	sockaddr *addr = (sockaddr *)&net->address;
	int error      = connect(net->descriptor, addr, (int)net->addrlen);
	if (error) {
		PL_Net_ReportLastSocketError();
		return false;
	}
	return PL_Net_OpenSSLReconnect(net);
}

String Net_GetHostname(Net_Socket *net) {
	return String(net->hostname, net->hostlen);
}

int Net_GetPort(Net_Socket *net) {
	sockaddr_in *addr = (sockaddr_in *)&net->address;
	return ntohs(addr->sin_port);
}

int32_t Net_GetSocketDescriptor(Net_Socket *net) {
	return (int32_t)net->descriptor;
}

bool Net_SetSocketBlockingMode(Net_Socket *net, bool blocking) {
	SOCKET fd = net->descriptor;
#if PLATFORM_WINDOWS
	u_long mode = blocking ? 0 : 1;
	return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#elif PLATFORM_LINUX || PLATFORM_MAC
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) return false;
	flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
	return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
#endif
}

int Net_SendBlocked(Net_Socket *net, void *buffer, int length, int timeout) {
	pollfd fds = {};
	fds.fd = net->descriptor;
	fds.events = POLLWRNORM;

	int presult = poll(&fds, 1, timeout);

	if (presult > 0) {
		if (fds.revents & POLLWRNORM) {
			int written = net->write(net, buffer, length);
			if (written >= 0) {
				net->error = NET_E_NONE;
				return written;
			}
			net->error = NET_E_CONNECTION_LOST;
#ifdef NETWORK_OPENSSL_ENABLE
			if (net->ssl)
				PL_Net_ReportOpenSSLError();
			else
				PL_Net_ReportLastSocketError();
#else
				PL_Net_ReportLastSocketError();
#endif
			return -1;
		}

		if (fds.revents & (POLLHUP | POLLERR)) {
			net->error = NET_E_CONNECTION_LOST;
			return -1;
		}
		net->error = NET_E_WOULD_BLOCK;
		return 0;
	} else if (presult == 0) {
		net->error = NET_E_TIMED_OUT;
		return -1;
	}

	net->error = NET_E_CONNECTION_LOST;
	PL_Net_ReportLastPlatformError();
	return -1;
}

int Net_ReceiveBlocked(Net_Socket *net, void *buffer, int length, int timeout) {
	pollfd fds = {};
	fds.fd = net->descriptor;
	fds.events = POLLRDNORM;

	int presult = poll(&fds, 1, timeout);

	if (presult > 0) {
		if (fds.revents & POLLRDNORM) {
			int read = net->read(net, buffer, length);
			if (read > 0)
				return read;
			if (read == 0) LogErrorEx("Net", "Lost connection unexpectedly");
#ifdef NETWORK_OPENSSL_ENABLE
			if (net->ssl)
				PL_Net_ReportOpenSSLError();
			else
				PL_Net_ReportLastSocketError();
#else
			PL_Net_ReportLastSocketError();
#endif
			net->error = NET_E_CONNECTION_LOST;
			return -1;
		}

		if (fds.revents & (POLLHUP | POLLERR)) {
			net->error = NET_E_CONNECTION_LOST;
			return -1;
		}
		net->error = NET_E_WOULD_BLOCK;
		return 0;
	} else if (presult == 0) {
		net->error = NET_E_WOULD_BLOCK;
		return 0;
	}

	net->error = NET_E_CONNECTION_LOST;
	PL_Net_ReportLastPlatformError();
	return -1;
}

int Net_Send(Net_Socket *net, void *buffer, int length) {
	int written = net->write(net, buffer, length);
	if (written < 0) {
#if PLATFORM_WINDOWS
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			net->error = NET_E_WOULD_BLOCK;
			return 0;
		}
#elif PLATFORM_LINUX || PLATFORM_MAC
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			net->error = NET_E_WOULD_BLOCK;
			return 0;
		}
#endif
		net->error = NET_E_CONNECTION_LOST;
		PL_Net_ReportLastSocketError();
		return -1;
	}
	net->error = NET_E_NONE;
	return written;
}

int Net_Receive(Net_Socket *net, void *buffer, int length) {
	int read = net->read(net, buffer, length);
	if (read <= 0) {
#if PLATFORM_WINDOWS
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			net->error = NET_E_WOULD_BLOCK;
			return 0;
		}
#elif PLATFORM_LINUX || PLATFORM_MAC
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			net->error = NET_E_WOULD_BLOCK;
			return 0;
		}
#endif
		if (read == 0) LogErrorEx("Net", "Lost connection unexpectedly");
		net->error = NET_E_CONNECTION_LOST;
		PL_Net_ReportLastSocketError();
		return -1;
	}
	return read;
}
