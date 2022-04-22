#include "Kr/KrBasic.h"
#include "StringBuilder.h"
#include "Network.h"
#include "Discord.h"

#include <stdio.h>

#include "Json.h"

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//

static const String Message = u8R"foo(
{
  "content": "Hello, Discord!🔥🔥🔥🔥🔥",
  "tts": false,
  "embeds": [{
    "title": "Hello, Embed!",
    "description": "This is an embedded message."
  }]
}
)foo";

void LogProcedure(void *context, Log_Level level, const char *source, const char *fmt, va_list args) {
	FILE *fp = level == LOG_LEVEL_INFO ? stdout : stderr;
	fprintf(fp, "[%s] ", source);
	vfprintf(fp, fmt, args);
	fprintf(fp, "\n");
}

int main(int argc, char **argv) {
	InitThreadContext(MegaBytes(64));
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	Net_Socket *net = Net_OpenConnection("discord.com", "443", NET_SOCKET_TCP);
	if (!net) {
		return 1;
	}

	Net_OpenSecureChannel(net);

	const char *token = argv[1];

	String_Builder builder;

	String method = "POST";
	String endpoint = "/api/v9/channels/850062383266136065/messages";

	WriteFormatted(&builder, "% % HTTP/1.1\r\n", method, endpoint);
	WriteFormatted(&builder, "Authorization: Bot %\r\n", token);
	Write(&builder, "User-Agent: KatachiBot\r\n");
	Write(&builder, "Connection: keep-alive\r\n");
	WriteFormatted(&builder, "Host: %\r\n", Net_GetHostname(net));
	WriteFormatted(&builder, "Content-Type: %\r\n", "application/json");
	WriteFormatted(&builder, "Content-Length: %\r\n", Message.length);
	Write(&builder, "\r\n");
	WriteBuffer(&builder, Message.data, Message.length);

	// Send header
	int bytes_sent = 0;
	for (auto buk = &builder.head; buk; buk = buk->next) {
		bytes_sent += Net_Write(net, buk->data, (int)buk->written);
	}

	static char buffer[4096 * 2];

	int bytes_received = 0;
	bytes_received += Net_Read(net, buffer, sizeof(buffer) - 1);
	bytes_received += Net_Read(net, buffer + bytes_received, sizeof(buffer) - 1 - bytes_received);
	if (bytes_received < 1) {
		Unimplemented();
	}

	buffer[bytes_received] = 0;

	printf("\n%s\n", buffer);


	Net_CloseConnection(net);

	Net_Shutdown();

	return 0;
}
