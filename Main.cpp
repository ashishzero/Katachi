#include "Basic.h"
#include "StringBuilder.h"
#include "Network.h"
#include "Discord.h"

#include <stdio.h>

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//

struct Json_Builder {
	String_Builder *builder;
	uint64_t depth;
};

int JsonNextElement(Json_Builder *json) {
	int written = 0;
	if (json->depth & 0x1) {
		written = Write(json->builder, ",");
	} else {
		json->depth |= 0x1;
	}
	return written;
}

int JsonBeginObject(Json_Builder *json) {
	//Assert(json->depth & 0x8000000000000000 == 0);
	int written = JsonNextElement(json);
	json->depth = json->depth << 1;
	return written + Write(json->builder, "{");
}

int JsonEndObject(Json_Builder *json) {
	json->depth = json->depth >> 1;
	return Write(json->builder, "}");
}

int JsonBeginArray(Json_Builder *json) {
	//Assert(json->depth & 0x8000000000000000 == 0);
	int written = JsonNextElement(json);
	json->depth = json->depth << 1;
	return written + Write(json->builder, "[");
}

int JsonEndArray(Json_Builder *json) {
	json->depth = json->depth >> 1;
	return Write(json->builder, "]");
}

int JsonKey(Json_Builder *json, String key) {
	int written = 0;
	written += JsonNextElement(json);
	written += Write(json->builder, '"');
	written += Write(json->builder, key);
	written += Write(json->builder, '"');
	written += Write(json->builder, ':');
	json->depth &= ~(uint64_t)0x1;
	return written;
}

template <typename Type>
int JsonKeyValue(Json_Builder *json, String key, Type value) {
	int written = 0;
	written += JsonKey(json, key);
	written += WriteFormatted(json->builder, "\"%\"", value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonKeyValue(Json_Builder *json, String key, bool value) {
	int written = 0;
	written += JsonKey(json, key);
	written += Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonKeyValue(Json_Builder *json, String key, int64_t value) {
	int written = 0;
	written += JsonKey(json, key);
	written += Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonKeyValue(Json_Builder *json, String key, uint64_t value) {
	int written = 0;
	written += JsonKey(json, key);
	written += Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <typename Type>
int JsonValue(Json_Builder *json, Type value) {
	int written = WriteFormatted(json->builder, "\"%\"", value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonValue(Json_Builder *json, bool value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonValue(Json_Builder *json, int64_t value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonValue(Json_Builder *json, uint64_t value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

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

	String_Builder content_builder;
	Json_Builder json = { &content_builder, 0 };

	JsonBeginObject(&json);
	JsonKeyValue(&json, "tts", false);
	JsonKey(&json, "embeds");

	JsonBeginArray(&json);

	JsonBeginObject(&json);
	JsonKeyValue(&json, "title", "Yahallo!");
	JsonKeyValue(&json, "description", "This message is generated from Katachi bot (programming in C)");
	JsonEndObject(&json);

	JsonEndArray(&json);
	JsonEndObject(&json);


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
		int bytes_sent = NetWriteSecured(&net, buk->data, buk->written);
	}

	// Send Content
	for (auto buk = &content_builder.head; buk; buk = buk->next) {
		int bytes_sent = NetWriteSecured(&net, buk->data, buk->written);
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
