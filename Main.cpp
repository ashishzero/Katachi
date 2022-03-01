#include "KrBasic.h"
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

int main(int argc, char **argv) {
	InitThreadContext(MegaBytes(64));

	STable<int> table;

	table.Put("one", 1);
	table.Put("two", 2);
	table.Put("three", 3);

	for (auto &p : table) { printf("\t%s: %d\n", p.key.data, p.value); }

	table.Remove("one");

	for (auto &p : table) { printf("\t%s: %d\n", p.key.data, p.value); }

	table.Put("zero", 0);

	for (auto &p : table) { printf("\t%s: %d\n", p.key.data, p.value); }

	auto two = table.FindOrPut("two");

	auto five = table.Find("five");

	auto zero = table.Find("zero");

	Free(&table);

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Json parsed_json;
	bool parsed = JsonParse(Message, &parsed_json);

	printf("JSON parsed: %s...\n", parsed ? "done" : "failed");

	NetInit();

	Net_Socket net;
	if (NetOpenClientConnection("discord.com", "443", &net) != Net_Ok) {
		return 1;
	}

	NetPerformTLSHandshake(&net);

	const char *token = argv[1];

	String_Builder content_builder;
	Json_Builder json = JsonBuilderCreate(&content_builder);
	JsonBuild(&json, parsed_json);

	String_Builder builder;

	String method = "POST";
	String endpoint = "/api/v9/channels/850062383266136065/messages";

	WriteFormatted(&builder, "% % HTTP/1.1\r\n", method, endpoint);
	WriteFormatted(&builder, "Authorization: Bot %\r\n", token);
	Write(&builder, "User-Agent: KatachiBot\r\n");
	Write(&builder, "Connection: keep-alive\r\n");
	WriteFormatted(&builder, "Host: %\r\n", net.info.hostname);
	WriteFormatted(&builder, "Content-Type: %\r\n", "application/json");
	WriteFormatted(&builder, "Content-Length: %\r\n", content_builder.written);
	Write(&builder, "\r\n");

	// Send header
	for (auto buk = &builder.head; buk; buk = buk->next) {
		int bytes_sent = NetWriteSecured(&net, buk->data, (int)buk->written);
	}

	// Send Content
	for (auto buk = &content_builder.head; buk; buk = buk->next) {
		int bytes_sent = NetWriteSecured(&net, buk->data, (int)buk->written);
	}

	static char buffer[4096 * 2];

	int bytes_received = NetReadSecured(&net, buffer, sizeof(buffer) - 1);
	bytes_received += NetReadSecured(&net, buffer + bytes_received, sizeof(buffer) - 1 - bytes_received);
	if (bytes_received < 1) {
		Unimplemented();
	}

	buffer[bytes_received] = 0;

	printf("\n%s\n", buffer);

	JsonFree(&parsed_json);

	NetCloseConnection(&net);

	NetShutdown();

	return 0;
}
