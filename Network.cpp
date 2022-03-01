#include "Network.h"

#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#if PLATFORM_WINDOWS
#include <Winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")

#pragma comment(lib, "openssl/libcrypto_static.lib")
#pragma comment(lib, "openssl/libssl_static.lib")
#endif

#if PLATFORM_LINUX || PLATFORM_MAC
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define closesocket close
#define SOCKET int
#define INVALID_SOCKET -1
#endif

static SSL_CTX *DefaultClientContext;

Net_Result NetInit() {
#if PLATFORM_WINDOWS
	WSADATA wsaData;
	int error = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (error != 0) {
		Unimplemented();
		return Net_Error;
	}
#endif

#if PLATFORM_LINUX || PLATFORM_MAC
	signal(SIGPIPE, SIG_IGN);
#endif

	SSL_library_init();
	OpenSSL_add_all_algorithms();

	DefaultClientContext = SSL_CTX_new(TLS_client_method());

	if (!DefaultClientContext) {
		Unimplemented();
		return Net_Error;
	}

	SSL_CTX_set_verify(DefaultClientContext, SSL_VERIFY_PEER, nullptr);

#if PLATFORM_WINDOWS
	X509_STORE *store = SSL_CTX_get_cert_store(DefaultClientContext);
	if (!store) {
		Unimplemented();
		return Net_Error;
	}

	HCERTSTORE cert_store = CertOpenSystemStoreW(0, L"ROOT");
	if (!cert_store) {
		Unimplemented();
		return Net_Error;
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
		Unimplemented();
		return Net_Error;
	}
#endif

	return Net_Ok;
}

void NetShutdown() {
	SSL_CTX_free(DefaultClientContext);

#if PLATFORM_WINDOWS
	WSACleanup();
#endif
}

//
//
//

static Net_Result NetConnect(Net_Socket *net) {
    addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo *address = nullptr;
	int error = getaddrinfo(net->info.hostname, net->info.port, &hints, &address);
	if (error) {
		Unimplemented();
		return Net_Error;
	}

	SOCKET socket_handle = INVALID_SOCKET;

	for (auto ptr = address; ptr; ptr = ptr->ai_next) {
		socket_handle = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (socket_handle == -1) {
			Unimplemented();
			freeaddrinfo(address);
			return Net_Error;
		}

		error = connect(socket_handle, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (error) {
			closesocket(socket_handle);
			socket_handle = INVALID_SOCKET;
			continue;
		}

		break;
	}

	freeaddrinfo(address);

	if (socket_handle == INVALID_SOCKET) {
		Unimplemented();
		return Net_Error;
	}

	net->descriptor = (int64_t)socket_handle;
	net->handle = nullptr;

	return Net_Ok;
}

static Net_Result NetReconnect(Net_Socket *net) {
	bool handshake = (net->handle != nullptr);
	NetCloseConnection(net);
	if (NetConnect(net) == Net_Ok) {
		if (handshake) {
			return NetPerformTLSHandshake(net);
		}
	}
	return Net_Ok;
}

Net_Result NetOpenClientConnection(const String hostname, const String port, Net_Socket *net) {
	Assert(hostname.length < ArrayCount(net->info.hostname) && port.length < 8);

    net->info.hostname_length = (uint16_t)hostname.length;
    memcpy(net->info.hostname, hostname.data, hostname.length);
    net->info.hostname[hostname.length] = 0;

	memcpy(net->info.port, port.data, port.length);
	net->info.port[port.length] = 0;
	
	net->handle = nullptr;

	return NetConnect(net);
}

Net_Result NetPerformTLSHandshake(Net_Socket *net) {
	SSL *ssl = SSL_new(DefaultClientContext);

	if (!ssl) {
		Unimplemented();
		return Net_Error;
	}

	if (!SSL_set_tlsext_host_name(ssl, net->info.hostname)) {
		Unimplemented();
		return Net_Error;
	}

	SSL_set_fd(ssl, (int)net->descriptor);
	if (SSL_connect(ssl) == -1) {
		Unimplemented();
		return Net_Error;
	}

	X509 *x509 = SSL_get_peer_certificate(ssl);
	Defer{ X509_free(x509); };
	if (!x509) {
		Unimplemented();
		return Net_Error;
	}

	long res = SSL_get_verify_result(ssl);
	if (res != X509_V_OK) {
		Unimplemented();
		return Net_Error;
	}

	net->handle = ssl;

	return Net_Ok;
}

void NetCloseConnection(Net_Socket *net) {
	if (net->handle) {
		SSL *ssl = (SSL *)net->handle;
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
	closesocket((SOCKET)net->descriptor);
	net->handle = nullptr;
	net->descriptor = INVALID_SOCKET;
}

int NetWrite(Net_Socket *net, void *buffer, int length) {
	int written = send((SOCKET)net->descriptor, (char *)buffer, length, 0);

	if (written <= 0) {
		#if PLATFORM_WINDOWS
		bool should_reconnect = (WSAGetLastError() == WSAECONNRESET);
		#else
		bool should_reconnect = (errno == EPIPE);
		#endif

		if (should_reconnect && NetReconnect(net) == Net_Ok) {
			written = send((SOCKET)net->descriptor, (char *)buffer, length, 0);
		}
	}

	return written;
}

int NetRead(Net_Socket *net, void *buffer, int length) {
	return recv((SOCKET)net->descriptor, (char *)buffer, length, 0);
}

int NetWriteSecured(Net_Socket *net, void *buffer, int length) {
	int written = SSL_write((SSL *)net->handle, (char *)buffer, length);

	if (written <= 0 && SSL_get_error((SSL *)net->handle, written) == SSL_ERROR_WANT_CONNECT) {
		if (NetReconnect(net) == Net_Ok) {
			written = SSL_write((SSL *)net->handle, (char *)buffer, length);
		}
	}

	return written;
}

int NetReadSecured(Net_Socket *net, void *buffer, int length) {
	return SSL_read((SSL *)net->handle, buffer, length);
}
