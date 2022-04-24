#include "Kr/KrBasic.h"
#include "Network.h"
#include "Discord.h"
#include "Kr/KrString.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Json.h"
#include "SHA1/sha1.h"

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

//
// https://discord.com
//

#include "Http.h"

void Dump(const Http_Header &header, uint8_t *buffer, int length, void *content) {
	printf("%.*s", length, buffer);
}

static const String Base64Lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr size_t Base64EncodedSize(size_t size) {
	ptrdiff_t base64_len = (4 * size) / 3;
	return AlignPower2Up(base64_len, 4);
}

constexpr size_t Based64DecodedSize(size_t size) {
	return (3 * size) / 4;
}

String EncodeBase64(Buffer input, uint8_t *base64, ptrdiff_t length) {
	ptrdiff_t base64_len = Base64EncodedSize(input.length);
	Assert(base64_len <= length);

	int val = 0;
	int valb = -6;

	ptrdiff_t pos = 0;
	String result(base64, base64_len);

	for (ptrdiff_t index = 0; index < input.length; ++index) {
		uint8_t c = input[index];
		val = (val << 8) + static_cast<uint8_t>(c);
		valb += 8;
		while (valb >= 0) {
			result[pos++] = Base64Lookup[(val >> valb) & 0x3F];
			valb -= 6;
		}
	}

	if (valb > -6) {
		result[pos++] = Base64Lookup[((val << 8) >> (valb + 8)) & 0x3F];
	}

	for (; pos < base64_len; ++pos)
		result[pos] = '=';

	return result;
}

Buffer DecodeBase64(String base64, uint8_t *buffer, ptrdiff_t length) {
	while (base64[base64.length - 1] == '=')
		base64.length -= 1;

	Buffer result;
	result.length = Based64DecodedSize(base64.length);
	result.data   = buffer;

	Assert(result.length <= length);

	uint8_t positions[4];
	ptrdiff_t pos = 0;
	ptrdiff_t index = 0;

	for (; index < base64.length - 4; index += 4) {
		positions[0] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index + 0]);
		positions[1] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index + 1]);
		positions[2] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index + 2]);
		positions[3] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index + 3]);

		result[pos++] = (positions[0] << 2) + ((positions[1] & 0x30) >> 4);
		result[pos++] = ((positions[1] & 0xf) << 4) + ((positions[2] & 0x3c) >> 2);
		result[pos++] = ((positions[2] & 0x3) << 6) + positions[3];
	}

	ptrdiff_t it = 0;
	for (; index < base64.length; ++index, ++it)
		positions[it] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index]);
	for (; it < 4; ++it)
		positions[it] = 0;

	uint8_t values[3];
	values[0] = (positions[0] << 2) + ((positions[1] & 0x30) >> 4);
	values[1] = ((positions[1] & 0xf) << 4) + ((positions[2] & 0x3c) >> 2);
	values[2] = ((positions[2] & 0x3) << 6) + positions[3];

	for (it = 0; pos < result.length; ++pos, ++it)
		result[pos] = values[it];

	return result;
}

static constexpr int SecureWebSocketKeySize = 24;

static String GenerateSecureWebSocketKey(uint8_t(&buffer)[SecureWebSocketKeySize]) {
	uint8_t nonce[16];
	for (auto &n : nonce)
		n = 'a'; //(uint32_t)rand();
	static_assert(sizeof(nonce) == 16, "");
	return EncodeBase64(Buffer((uint8_t *)nonce, sizeof(nonce)), buffer, sizeof(buffer));
}

static int Websocket_ReadMinimum(Net_Socket *websocket, uint8_t *read_ptr, int buffer_size, int minimum_req) {
	Assert(minimum_req <= buffer_size);
	int bytes_received = 0;
	while (bytes_received < minimum_req) {
		int bytes_read = Net_Read(websocket, read_ptr, buffer_size);
		if (bytes_read < 0)
			return bytes_read;
		bytes_received += bytes_read;
		read_ptr += bytes_read;
		buffer_size -= bytes_read;
	}
	return bytes_received;
}

int main(int argc, char **argv) {
	InitThreadContext(KiloBytes(64));
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	//srand(time(nullptr));

	Memory_Arena *arena = MemoryArenaAllocate(MegaBytes(64));

	Net_Socket *http = Http_Connect("https://discord.com");
	if (!http) {
		return 1;
	}

	const String token = FmtStr(arena, "Bot %s", argv[1]);

	Http_Request *req = Http_CreateRequest(http, arena);
	Http_SetHeader(req, HTTP_HEADER_AUTHORIZATION, token);
	Http_SetHeader(req, HTTP_HEADER_USER_AGENT, "Katachi");
	Http_SetHeader(req, HTTP_HEADER_CONNECTION, "keep-alive");
	//Http_RequestSetContent(req, "application/json", Message);

	Http_Body_Writer writer = { Dump };

	uint8_t read_buffer[100] = {};
	int bytes_sent = Net_Read(http, read_buffer, sizeof(read_buffer));

	Http_Response *res = Http_Get(http, "/api/v9/gateway/bot", req);
	
	if (res) {
		printf("%.*s\n\n", (int)res->body.length, res->body.data);

		Http_DestroyResponse(res);
	}

	Http_Disconnect(http);

	// wss://gateway.discord.gg/?v=9&encoding=json
	
	Net_Socket *websocket = Http_Connect("wss://gateway.discord.gg", HTTPS_CONNECTION);
	if (!websocket) {
		return 1;
	}

	uint8_t buffer[SecureWebSocketKeySize];
	String wsskey = GenerateSecureWebSocketKey(buffer);

	req = Http_CreateRequest(websocket, arena);
	Http_SetHeader(req, HTTP_HEADER_AUTHORIZATION, token);
	Http_SetHeader(req, HTTP_HEADER_USER_AGENT, "Katachi");
	Http_SetHeader(req, HTTP_HEADER_UPGRADE, "websocket");
	Http_SetHeader(req, HTTP_HEADER_CONNECTION, "Upgrade");
	Http_SetCustomHeader(req, "Sec-WebSocket-Version", "13");
	Http_SetCustomHeader(req, "Sec-WebSocket-Key", wsskey);


	res = Http_Get(websocket, "/?v=9&encoding=json", req);

	if (res) {
		if (res->status.code == 101) {
			printf("%.*s\n\n", (int)res->body.length, res->body.data);

			String given = Http_GetCustomHeader(res, "Sec-WebSocket-Accept");
			if (!given.length) return 1;

			constexpr char WebsocketKeySalt[]  = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
			constexpr int WebsocketKeySaltSize = sizeof(WebsocketKeySalt) - 1;

			uint8_t combined[SecureWebSocketKeySize + WebsocketKeySaltSize];
			memcpy(combined, wsskey.data, SecureWebSocketKeySize);
			memcpy(combined + SecureWebSocketKeySize, WebsocketKeySalt, WebsocketKeySaltSize);

			uint8_t digest[20];
			SHA1((char *)digest, (char *)combined, SecureWebSocketKeySize + WebsocketKeySaltSize);

			uint8_t buffer_base64[Base64EncodedSize(20)];
			String required = EncodeBase64(String(digest, sizeof(digest)), buffer_base64, sizeof(buffer_base64));

			if (required != given) {
				WriteLogErrorEx("WSS", "Server returned invalid accept value");
			} else {
				WriteLogInfo("Websocket connected.");
			}

			constexpr int WEBSOCKET_STREAM_SIZE = KiloBytes(8);

			uint8_t stream[WEBSOCKET_STREAM_SIZE] = {};

			uint8_t *read_ptr = stream;

			int bytes_received = Websocket_ReadMinimum(websocket, read_ptr, WEBSOCKET_STREAM_SIZE, 2);

			enum Websocket_Opcode {
				WEBSOCKET_OP_CONTINUATION_FRAME = 0x0,
				WEBSOCKET_OP_TEXT_FRAME = 0x1,
				WEBSOCKET_OP_BINARY_FRAME = 0x2,
				WEBSOCKET_OP_CONNECTION_CLOSE = 0x8,
				WEBSOCKET_OP_PING = 0x9,
				WEBSOCKET_OP_PONG = 0xa
			};

			int fin = (read_ptr[0] & 0x80) >> 7;
			int rsv = (read_ptr[0] & 0x70) >> 4; // must be zero, since we don't use extensions
			int opcode = (read_ptr[0] & 0x0f);
			int mask = (read_ptr[1] & 0x80) >> 7; // must be zero for client, must be 1 when sending data to server

			uint64_t payload_len = (read_ptr[1] & 0x7f);

			if (payload_len < 126) {
				read_ptr += 2;
				bytes_received -= 2;
			} else if (payload_len == 126) {
				if (bytes_received < 4) {
					bytes_received += Websocket_ReadMinimum(websocket, read_ptr + bytes_received, WEBSOCKET_STREAM_SIZE - bytes_received, 4 - bytes_received);
				}

			}

			int fuck_visual_studio = 42;
		}

		Http_DestroyResponse(res);
	}

	Http_Disconnect(websocket);

	Net_Shutdown();

	return 0;
}
