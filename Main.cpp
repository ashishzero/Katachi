#include "Basic.h"
#include "StringBuilder.h"
#include "Network.h"

#include <stdio.h>

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	InitThreadContext(MegaBytes(64));

	NetInit();

	Net_Socket net;
	if (NetOpenClientConnection("discord.com", "443", &net) != Net_Ok) {
		return 1;
	}

	NetPerformTSLHandshake(&net);

	const char *token = argv[1];

	const String content = 
R"foo-bar(
{
  "tts": false,
  "embeds": [{
    "title": "Yahallo!",
    "description": "This message is generated from Katachi bot (programming in C)"
  }]
}
)foo-bar";

	String_Builder builder;

	String method = "POST";
	String endpoint = "/api/v9/channels/850062383266136065/messages";

	WriteFormatted(&builder, "% % HTTP/1.1\r\n", method, endpoint);
	WriteFormatted(&builder, "Authorization: Bot %\r\n", token);
	Write(&builder, "User-Agent: KatachiBot\r\n");
	Write(&builder, "Connection: keep-alive\r\n");
	WriteFormatted(&builder, "Host: %\r\n", net.info.hostname);
	WriteFormatted(&builder, "Content-Type: %\r\n", "application/json");
	WriteFormatted(&builder, "Content-Length: %\r\n", content.length);
	Write(&builder, "\r\n");

	// Send header
	for (auto buk = &builder.head; buk; buk = buk->next) {
		int bytes_sent = NetWriteSecured(&net, buk->data, buk->written);
	}

	// Send Content
	{
		int bytes_sent = NetWriteSecured(&net, content.data, (int)content.length);
	}

	static char buffer[4096 * 2];

	int bytes_received = NetReadSecured(&net, buffer, sizeof(buffer) - 1);
	bytes_received += NetReadSecured(&net, buffer + bytes_received, sizeof(buffer) - 1 - bytes_received);
	if (bytes_received < 1) {
		Unimplemented();
	}

	buffer[bytes_received] = 0;

	printf("\n%s\n", buffer);

	NetCloseConnection(&net);

	NetShutdown();

	return 0;
}
