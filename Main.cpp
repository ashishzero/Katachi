#include "Kr/KrBasic.h"
#include "Network.h"
#include "Discord.h"
#include "Kr/KrString.h"
#include "StringBuilder.h"
#include "Websocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Json.h"


//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//

// The first reconnect attempt SHOULD be delayed by a random amount of
// time.The parameters by which this random delay is chosen are left
// to the client to decide; a value chosen randomly between 0 and 5
// seconds is a reasonable initial delay though clients MAY choose a
// different interval from which to select a delay length based on
// implementation experience and particular application.
// Should the first reconnect attempt fail, subsequent reconnect
// attempts SHOULD be delayed by increasingly longer amounts of time,
// using a method such as truncated binary exponential backoff.


void LogProcedure(void *context, Log_Level level, const char *source, const char *fmt, va_list args) {
	FILE *fp = level == LOG_LEVEL_INFO ? stdout : stderr;
	fprintf(fp, "[%s] ", source);
	vfprintf(fp, fmt, args);
	fprintf(fp, "\n");
}

namespace Discord {
	enum Gateway_Opcode {
		GATEWAY_OP_DISPATH = 0,
		GATEWAY_OP_HEARTBEAT = 1,
		GATEWAY_OP_IDENTIFY = 2,
		GATEWAY_OP_PRESECE_UPDATE = 3,
		GATEWAY_OP_VOICE_STATE_UPDATE = 4,
		GATEWAY_OP_RESUME = 6,
		GATEWAY_OP_RECONNECT = 7,
		GATEWAY_OP_REQUEST_GUILD_MEMBERS = 8,
		GATEWAY_OP_INVALID_SESSION = 9,
		GATEWAY_OP_HELLO = 10,
		GATEWAY_OP_HEARTBEAT_ACK = 11,
	};

	enum Client_State {
		CLIENT_CONNECTING,
		CLIENT_CONNECTED,

		_CLIENT_STATE_COUNT
	};

	struct Client {
		Client_State state = CLIENT_CONNECTING;

		int heartbeats = 0;
		int acknowledgements = 0;
		int sequence = -1;

		String token;
		String authorization;

		Memory_Arena *arena = nullptr;
	};
}

static void Discord_Hearbeat(Websocket *websocket, Discord::Client *client) {
	if (client->heartbeats != client->acknowledgements) {
		// @todo: only wait for certain number of times
		LogWarningEx("Discord", "Acknowledgement not reveived (%d/%d)", client->heartbeats, client->acknowledgements);
	}

	int buffer_cap  = 4096;
	uint8_t *buffer = PushArray(client->arena, uint8_t, buffer_cap);

	int length = 0;
	if (client->sequence > 0)
		length = snprintf((char *)buffer, buffer_cap, "{\"op\": 1, \"d\":%d}", client->sequence);
	else
		length = snprintf((char *)buffer, buffer_cap, "{\"op\": 1, \"d\":null}");
	Websocket_SendText(websocket, Buffer(buffer, length));
	client->heartbeats += 1;

	Trace("Heartbeat (%d)", client->heartbeats);
}

static void Discord_HandleWebsocketEvent(Websocket *websocket, const Websocket_Event &event, void *user_context) {
	Discord::Client *client = (Discord::Client *)user_context;

	if (event.type == WEBSOCKET_EVENT_TEXT) {
		auto temp = BeginTemporaryMemory(client->arena);
		Defer{ EndTemporaryMemory(&temp); };

		String message = event.message;

		Json json;
		if (!JsonParse(message, &json, MemoryArenaAllocator(client->arena))) {
			LogError("Invalid Event received: " StrFmt, StrArg(message));
			return;
		}

		Json_Object obj  = JsonGetObject(json);
		client->sequence = JsonGetInt(obj, "s", client->sequence);

		int opcode       = JsonGetInt(obj, "op");

		switch (client->state) {
			case Discord::CLIENT_CONNECTING: {
				if (opcode == Discord::GATEWAY_OP_HELLO) {
					Json_Object d = JsonGetObject(obj, "d");
					int heartbeat_interval = JsonGetInt(d, "heartbeat_interval", 45000);
					Websocket_SetTimeout(websocket, heartbeat_interval);

					client->state = Discord::CLIENT_CONNECTED;

					int buffer_cap = 4096;
					uint8_t *buffer = PushArray(client->arena, uint8_t, buffer_cap);
					int length = snprintf((char *)buffer, buffer_cap, R"foo(
{
  "op": 2,
  "d": {
    "token": "%.*s",
    "intents": 513,
    "properties": {
      "$os": "Windows"
    }
  }
}
)foo", StrArg(client->token));

					Websocket_SendText(websocket, Buffer(buffer, length));
				}
			} break;

			case Discord::CLIENT_CONNECTED: {
				if (opcode == Discord::GATEWAY_OP_HEARTBEAT) {
					Discord_Hearbeat(websocket, client);
				} else if (opcode == Discord::GATEWAY_OP_HEARTBEAT_ACK) {
					client->acknowledgements += 1;
					Trace("Acknowledgement (%d)", client->acknowledgements);
				}
			} break;
		}

		return;
	}

	if (event.type == WEBSOCKET_EVENT_TIMEOUT) {
		Discord_Hearbeat(websocket, client);
		return;
	}
}

namespace Discord {
	const String UserAgent = "Katachi (https://github.com/Zero5620/Katachi, 0.1.1)";
}

int main(int argc, char **argv) {
	InitThreadContext(MegaBytes(64));
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	Discord::Client client;
	client.arena = MemoryArenaAllocate(MegaBytes(1));

	if (!client.arena) {
		return 1; // @todo log error
	}

	client.token         = FmtStr(client.arena, "%s", argv[1]);
	client.authorization = FmtStr(client.arena, "Bot %s", argv[1]);

	Http *http = Http_Connect("https://discord.com");
	if (!http) {
		return 1;
	}

	auto temp = BeginTemporaryMemory(client.arena);

	Http_Request req;
	Http_InitRequest(&req);
	Http_SetHost(&req, http);
	Http_SetHeader(&req, HTTP_HEADER_AUTHORIZATION, client.authorization);
	Http_SetHeader(&req, HTTP_HEADER_USER_AGENT, Discord::UserAgent);

	Http_Response res;
	if (!Http_Get(http, "/api/v9/gateway/bot", req, &res, client.arena)) {
		return 1; // @todo EndTemporaryMemory? MemoryArenaFree? Http_Disconnect?
	}

	Json json;
	if (!JsonParse(res.body, &json, MemoryArenaAllocator(client.arena))) {
		return 1; // @todo log EndTemporaryMemory? MemoryArenaFree? Http_Disconnect?
	}

	const Json_Object obj = JsonGetObject(json);

	if (res.status.code != 200) {
		String msg = JsonGetString(obj, "message");
		LogErrorEx("Discord", "Code: %u, Message: " StrFmt, res.status.code, StrArg(msg));
		return 1; // @todo log EndTemporaryMemory? MemoryArenaFree? Http_Disconnect?
	}

	Http_Disconnect(http);

	String url = JsonGetString(obj, "url");
	int shards = JsonGetInt(obj, "shards");
	// @todo: Handle sharding
	Trace("Getway URL: " StrFmt ", Shards: %d", StrArg(url), shards);

	Websocket_Header headers;
	Websocket_InitHeader(&headers);
	Websocket_HeaderSet(&headers, HTTP_HEADER_AUTHORIZATION, client.authorization);
	Websocket_HeaderSet(&headers, HTTP_HEADER_USER_AGENT, Discord::UserAgent);

	Websocket *websocket = Websocket_Connect(url, &res, &headers);
	if (!websocket) {
		return 1;
	}

	EndTemporaryMemory(&temp);

	Websocket_Loop(websocket, Discord_HandleWebsocketEvent, &client);

	Net_Shutdown();

	return 0;
}
