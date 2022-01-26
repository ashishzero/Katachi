#include <Winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")

#include "openssl/ssl.h"
#include "openssl/err.h"

#pragma comment(lib, "openssl/libcrypto_static.lib")
#pragma comment(lib, "openssl/libssl_static.lib")

#include "Common.h"
#include "StringBuilder.h"

#include <stdio.h>
#include <string.h>

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//


void LogError(const char *agent, const char *fmt, ...) {
	fprintf(stderr, "[%s] Error:", agent);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

bool NetInit() {
	WSADATA wsaData;
	int error = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (error != 0) {
		if (error == WSASYSNOTREADY)
			LogError("Network/WSAStartup", "The underlying network subsystem is not ready for network communication.");
		else if (error == WSAVERNOTSUPPORTED)
			LogError("Network/WSAStartup", "The version of Windows Sockets support requested is not provided by this particular Windows Sockets implementation.");
		else if (error == WSAEINPROGRESS)
			LogError("Network/WSAStartup", "A blocking Windows Sockets 1.1 operation is in progress.");
		else if (error == WSAEPROCLIM)
			LogError("Network/WSAStartup", "A limit on the number of tasks supported by the Windows Sockets implementation has been reached.");
		else if (error == WSAEFAULT)
			TriggerBreakpoint();
		else
			Unreachable();

		return false;
	}

	SSL_library_init();
	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	return true;
}

void NetShutdown() {
	WSACleanup();
}

struct Net_Socket {
	SOCKET handle;

	struct {
		SSL *handle;
		SSL_CTX *context;
	} ssl;

	String hostname;
	String port;

	char hostname_buffer[256];
	char port_buffer[8];
};

bool NetOpenConnection(const String hostname, const String port, Net_Socket *net) {
	Assert(hostname.length < ArrayCount(net->hostname_buffer) && port.length < ArrayCount(net->port_buffer));

	memcpy(net->hostname_buffer, hostname.data, hostname.length);
	memcpy(net->port_buffer, port.data, port.length);

	net->hostname_buffer[hostname.length] = 0;
	net->port_buffer[port.length] = 0;

	net->hostname = String((uint8_t *)net->hostname_buffer, hostname.length);
	net->port = String((uint8_t *)net->port_buffer, port.length);

	ADDRINFOA hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ADDRINFOA *address = nullptr;
	int result = GetAddrInfoA(net->hostname_buffer, net->port_buffer, &hints, &address);
	if (result) {
		Unimplemented();
		return false;
	}

	SOCKET connect_socket = INVALID_SOCKET;

	for (auto ptr = address; ptr; ptr = ptr->ai_next) {
		connect_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

		if (connect_socket == INVALID_SOCKET) {
			Unimplemented();
		}

		result = connect(connect_socket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (result == SOCKET_ERROR) {
			closesocket(connect_socket);
			connect_socket = INVALID_SOCKET;
			continue;
		}

		break;
	}

	FreeAddrInfoA(address);

	if (connect_socket == INVALID_SOCKET) {
		Unimplemented();
		return false;
	}

	net->handle = connect_socket;
	net->ssl.handle = NULL;
	net->ssl.context = NULL;

	return true;
}

bool NetPerformTSLHandshake(Net_Socket *net, bool verify = true) {
	net->ssl.context = SSL_CTX_new(TLS_client_method());

	if (!net->ssl.context) {
		Unimplemented();
		return false;
	}

	net->ssl.handle = SSL_new(net->ssl.context);

	if (!net->ssl.handle) {
		Unimplemented();
		return false;
	}

	if (!SSL_set_tlsext_host_name(net->ssl.handle, net->hostname_buffer)) {
		Unimplemented();
		return false;
	}

	SSL_set_fd(net->ssl.handle, (int)net->handle);
	if (SSL_connect(net->ssl.handle) == -1) {
		ERR_print_errors_fp(stderr);
		Unimplemented();
		return false;
	}

	X509 *x509 = SSL_get_peer_certificate(net->ssl.handle);
	if (x509 == NULL) {
		Unimplemented();
		return false;
	}
	Defer{ X509_free(x509); };

	if (verify) {
		auto arena = ThreadScratchpad();
		auto temp = BeginTemporaryMemory(arena);
		Defer{ EndTemporaryMemory(&temp); };

		int encoded_cert_len = i2d_X509(x509, NULL);
		uint8_t *encoded_cert = (uint8_t *)PushSize(arena, encoded_cert_len);

		if (!encoded_cert) {
			Unimplemented();
		}

		auto p = encoded_cert;
		i2d_X509(x509, &p);

		const CERT_CONTEXT *cert_context = CertCreateCertificateContext(X509_ASN_ENCODING, encoded_cert, encoded_cert_len);
		if (cert_context == NULL) {
			Unimplemented();
			return false;
		}
		Defer{ CertFreeCertificateContext(cert_context); };

		CERT_CHAIN_PARA chain_para;
		memset(&chain_para, 0, sizeof(chain_para));
		chain_para.cbSize = sizeof(chain_para);
		chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
		chain_para.RequestedUsage.Usage.cUsageIdentifier = 0;
		chain_para.RequestedUsage.Usage.rgpszUsageIdentifier = NULL;

		const CERT_CHAIN_CONTEXT *certificate_chain = nullptr;
		if (!CertGetCertificateChain(NULL, cert_context, NULL, NULL, &chain_para, CERT_CHAIN_TIMESTAMP_TIME, NULL, &certificate_chain)) {
			Unimplemented();
			return false;
		}
		Defer{ CertFreeCertificateChain(certificate_chain); };

		CERT_CHAIN_POLICY_PARA chain_policy_para;
		memset(&chain_policy_para, 0, sizeof(chain_policy_para));
		chain_policy_para.cbSize = sizeof(chain_policy_para);

		AUTHENTICODE_EXTRA_CERT_CHAIN_POLICY_STATUS auth_code = {};

		CERT_CHAIN_POLICY_STATUS policy_status;
		memset(&policy_status, 0, sizeof(policy_status));
		policy_status.cbSize = sizeof(policy_status);
		policy_status.pvExtraPolicyStatus = &auth_code;

		if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, certificate_chain, &chain_policy_para, &policy_status)) {
			Unimplemented();
			return false;
		}

		if (policy_status.dwError != 0) {
			Unimplemented();
			return false;
		}
	}

	return true;
}

void NetCloseConnection(Net_Socket *net) {
	if (net->ssl.context) {
		SSL_shutdown(net->ssl.handle);
		SSL_free(net->ssl.handle);
		SSL_CTX_free(net->ssl.context);
	}
	closesocket(net->handle);
}

int NetWrite(Net_Socket *net, void *buffer, int length) {
	return send(net->handle, (char *)buffer, length, 0);
}

int NetRead(Net_Socket *net, void *buffer, int length) {
	return recv(net->handle, (char *)buffer, length, 0);
}

int NetWriteEncrypted(Net_Socket *net, void *buffer, int length) {
	return SSL_write(net->ssl.handle, buffer, length);
}

int NetReadDecrypted(Net_Socket *net, void *buffer, int length) {
	return SSL_read(net->ssl.handle, buffer, length);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n", argv[0]);
		return 1;
	}

	InitThreadContext(MegaBytes(64));

	NetInit();

	Net_Socket net;
	if (!NetOpenConnection("discord.com", "443", &net)) {
		return 1;
	}

	NetPerformTSLHandshake(&net, true);

	const char *token = argv[1];

	const String content = 
R"foo-bar(
{
  "content": "Hello, From C!",
  "tts": false,
  "embeds": [{
    "title": "Hello, Embed!",
    "description": "This is an embedded message."
  }]
})foo-bar";

	String_Builder builder;

	String method = "POST";
	String endpoint = "/api/v9/channels/850062383266136065/messages";

	WriteFormatted(&builder, "% % HTTP/1.1\r\n", method, endpoint);
	WriteFormatted(&builder, "Authorization: Bot %\r\n", token);
	Write(&builder, "User-Agent: KatachiBot\r\n");
	Write(&builder, "Connection: keep-alive\r\n");
	WriteFormatted(&builder, "Host: %\r\n", net.hostname);
	WriteFormatted(&builder, "Content-Type: %\r\n", "application/json");
	WriteFormatted(&builder, "Content-Length: %\r\n", content.length);
	Write(&builder, "\r\n");

	// Send header
	for (auto buk = &builder.head; buk; buk = buk->next) {
		int bytes_sent = NetWriteEncrypted(&net, buk->data, buk->written);
	}

	// Send Content
	{
		int bytes_sent = NetWriteEncrypted(&net, content.data, (int)content.length);
	}

	static char buffer[4096 * 2];

	int bytes_received = NetReadDecrypted(&net, buffer, sizeof(buffer) - 1);
	bytes_received += NetReadDecrypted(&net, buffer + bytes_received, sizeof(buffer) - 1 - bytes_received);
	if (bytes_received < 1) {
		ERR_print_errors_fp(stdout);
		Unimplemented();
	}

	buffer[bytes_received] = 0;

	NetCloseConnection(&net);

	NetShutdown();

	return 0;
}
