#include <Winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Crypt32.lib")

#include "openssl/ssl.h"
#include "openssl/err.h"

#pragma comment(lib, "openssl/libcrypto_static.lib")
#pragma comment(lib, "openssl/libssl_static.lib")

#include <stdio.h>
#include <string.h>

#define TriggerBreakpoint() __debugbreak()
#define Assert(x) if (!x) { TriggerBreakpoint(); }
#define Unimplemented TriggerBreakpoint

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//

int main() {
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0) {
		// Tell the user that we could not find a usable Winsock DLL.
		Unimplemented();
	}

	SSL_library_init(); // TODO: Check error

	int result = 0;

	ADDRINFOW hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	ADDRINFOW *address = nullptr;
	result = GetAddrInfoW(L"discord.com", L"443", &hints, &address);
	if (result) {
		Unimplemented();
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

	FreeAddrInfoW(address);

	if (connect_socket == INVALID_SOCKET) {
		Unimplemented();
	}

	// TODO: Read docs + error
	OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
	SSL_load_error_strings();   /* Bring in and register error messages */
	auto method = TLSv1_2_client_method();  /* Create new client-method instance */
	auto ctx = SSL_CTX_new(method);   /* Create new context */
	if (ctx == NULL) {
		ERR_print_errors_fp(stdout);
		Unimplemented();
	}

	auto ssl = SSL_new(ctx);      /* create new SSL connection state */

	SSL_set_fd(ssl, connect_socket);    /* attach the socket descriptor */
	if (SSL_connect(ssl) == -1) {   /* perform the connection */
		ERR_print_errors_fp(stdout);
		Unimplemented();
	}
	else {
		auto xcert = SSL_get_peer_certificate(ssl); /* get the server's certificate */
		if (xcert == NULL) {
			Unimplemented();
		}

		unsigned char *cert_buf = NULL;
		int cert_len = i2d_X509(xcert, &cert_buf);
		if (cert_len < 0) {
			Unimplemented();
		}

		const CERT_CONTEXT *certificate = CertCreateCertificateContext(X509_ASN_ENCODING, cert_buf, cert_len);
		if (certificate == NULL) {
			Unimplemented();
		}

		CERT_CHAIN_PARA chain_para;
		memset(&chain_para, 0, sizeof(chain_para));
		chain_para.cbSize = sizeof(chain_para);
		chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
		chain_para.RequestedUsage.Usage.cUsageIdentifier = 0;
		chain_para.RequestedUsage.Usage.rgpszUsageIdentifier = NULL;

		const CERT_CHAIN_CONTEXT *certificate_chain = nullptr;
		if (!CertGetCertificateChain(NULL, certificate, NULL, NULL, &chain_para, CERT_CHAIN_TIMESTAMP_TIME, NULL, &certificate_chain)) {
			Unimplemented();
		}

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
		}

		if (policy_status.dwError != 0) {
			Unimplemented();
		}

		CertFreeCertificateChain(certificate_chain);
		CertFreeCertificateContext(certificate);

		X509_free(xcert);     /* free the malloc'ed certificate copy */
	}

	const char create_msg_json[] =
		R"foo-bar(
{
  "content": "Hello, From C!",
  "tts": false,
  "embeds": [{
    "title": "Hello, Embed!",
    "description": "This is an embedded message."
  }]
})foo-bar";


	// /api/v9/channels/850062383266136065
	const char *request =
		//"POST /api/v9/channels/850062383266136065/messages HTTP/1.1\r\n"
		"GET /api/v9/gateway/bot HTTP/1.1\r\n"
		"Authorization: Bot ODMzNjg2NjUxMTQ1NTUxOTIy.YH19Mg.ZlWmd1e1T74ENnKoLlrRNxJ5rCg\r\n"
		"User-Agent: KatachiBot\r\n"
		"Connection: keep-alive\r\n"
		"Host: discord.com\r\n"
		//"Content-Type: application/json\r\n"
		//"Content-Length: 151\r\n"
		"\r\n";

	int bytes_sent = SSL_write(ssl, request, strlen(request));
	//int bytes_sent = send(connect_socket, request, strlen(request), 0);
	if (bytes_sent < 1) {
		Unimplemented();
	}

	//bytes_sent = SSL_write(ssl, create_msg_json, strlen(create_msg_json));
	//if (bytes_sent < 1) {
	//	Unimplemented();
	//}

	static char buffer[4096 * 2];

	int bytes_received = SSL_read(ssl, buffer, sizeof(buffer) - 1);
	bytes_received += recv(connect_socket, buffer + bytes_received, sizeof(buffer) - 1 - bytes_received, 0);
	if (bytes_received < 1) {
		ERR_print_errors_fp(stdout);
		Unimplemented();
	}

	buffer[bytes_received] = 0;


	WSACleanup();

	return 0;
}
