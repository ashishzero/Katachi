#include "Network.h"

#include <string.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef PLATFORM_LINUX
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

static SSL_CTX *DefaultClientContext;

Net_Result NetInit() {
	SSL_library_init();
	OpenSSL_add_all_algorithms();

	DefaultClientContext = SSL_CTX_new(TLS_client_method());

	if (!DefaultClientContext) {
		Unimplemented();
		return Net_Error;
	}

	SSL_CTX_set_verify(DefaultClientContext, SSL_VERIFY_PEER, nullptr);

	long res = SSL_CTX_set_default_verify_paths(DefaultClientContext);

	if (res != 1) {
		Unimplemented();
		return Net_Error;
	}

	return Net_Ok;
}

void NetShutdown() {
	SSL_CTX_free(DefaultClientContext);
}

//
//
//

Net_Result NetOpenClientConnection(const String hostname, const String port, Net_Socket *net) {
	Assert(hostname.length < ArrayCount(net->info.hostname) && port.length < 8);

    net->info.hostname_length = (uint16_t)hostname.length;
    memcpy(net->info.hostname, hostname.data, hostname.length);
    net->info.hostname[hostname.length] = 0;

	char cport[8];
	memcpy(cport, port.data, port.length);
	cport[port.length] = 0;

	net->info.port = htons((uint16_t)atoi(cport));

    addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo *address = nullptr;
	int error = getaddrinfo(net->info.hostname, cport, &hints, &address);
	if (error) {
		Unimplemented();
		return Net_Error;
	}

	int socket_handle = -1;

	for (auto ptr = address; ptr; ptr = ptr->ai_next) {
		socket_handle = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (socket_handle == -1) {
			Unimplemented();
			freeaddrinfo(address);
			return Net_Error;
		}

		error = connect(socket_handle, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (error) {
			close(socket_handle);
			socket_handle = -1;
			continue;
		}

		break;
	}

	freeaddrinfo(address);

	if (socket_handle == -1) {
		Unimplemented();
		return Net_Error;
	}

	net->descriptor = (int64_t)socket_handle;
	net->handle = nullptr;

	return Net_Ok;
}

Net_Result NetPerformTSLHandshake(Net_Socket *net) {
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
	close((int)net->descriptor);
}

int NetWrite(Net_Socket *net, void *buffer, int length) {
	return send((int)net->descriptor, (char *)buffer, length, 0);
}

int NetRead(Net_Socket *net, void *buffer, int length) {
	return recv((int)net->descriptor, (char *)buffer, length, 0);
}

int NetWriteSecured(Net_Socket *net, void *buffer, int length) {
	return SSL_write((SSL *)net->handle, buffer, length);
}

int NetReadSecured(Net_Socket *net, void *buffer, int length) {
	return SSL_read((SSL *)net->handle, buffer, length);
}
