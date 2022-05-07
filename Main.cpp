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

	struct Client {
		bool hello_received = false;
		bool imm_heartbeat = false;
		bool identify = false;
		int heartbeat = 0;
		int acknowledgement = 0;
		int s = -1;
		const char *token = "";
	};
}

void OnWebsocketMessage(Websocket *websocket, Discord::Client *client, String payload) {
	Memory_Arena *arena = ThreadScratchpad();

	auto temp = BeginTemporaryMemory(arena);
	Defer{ EndTemporaryMemory(&temp); };

	Json json_event;
	if (!JsonParse(payload, &json_event, MemoryArenaAllocator(arena))) {
		LogError("Invalid JSON received: %.*s", (int)payload.length, payload.data);
		return;
	}
	Assert(json_event.type == JSON_TYPE_OBJECT);
	Json_Object *obj = &json_event.value.object;

	int opcode = (int)JsonObjectFind(obj, "op")->value.number.value.integer;

	auto s = JsonObjectFind(obj, "s");
	if (s && s->type == JSON_TYPE_NUMBER) {
		client->s = s->value.number.value.integer;
	}

	auto t = JsonObjectFind(obj, "t");
	if (t && t->type == JSON_TYPE_STRING) {
		String name = t->value.string;
		LogInfoEx("Discord Event", "%.*s", (int)name.length, name.data);
	}

	switch (opcode) {
		case Discord::GATEWAY_OP_HELLO: {
			auto d = &JsonObjectFind(obj, "d")->value.object;
			int timeout = JsonObjectFind(d, "heartbeat_interval")->value.number.value.integer;
			Websocket_SetTimeout(websocket, timeout);
			client->hello_received = true;
			client->identify = true;
			LogInfoEx("Discord Event", "Hello from senpai: %d", timeout);
		} break;

		case Discord::GATEWAY_OP_HEARTBEAT: {
			client->imm_heartbeat = true;
			LogInfoEx("Discord Event", "Call from senpai");
		} break;

		case Discord::GATEWAY_OP_HEARTBEAT_ACK: {
			client->acknowledgement += 1;
			LogInfoEx("Discord Event", "Senpai noticed me!");
		} break;

		case Discord::GATEWAY_OP_DISPATH: {
			LogInfoEx("Discord Event", "dispatch");
		} break;

		case Discord::GATEWAY_OP_IDENTIFY: {
			LogInfoEx("Discord Event", "identify");
		} break;

		case Discord::GATEWAY_OP_PRESECE_UPDATE: {
			LogInfoEx("Discord Event", "presence update");
		} break;

		case Discord::GATEWAY_OP_VOICE_STATE_UPDATE: {
			LogInfoEx("Discord Event", "voice state update");
		} break;

		case Discord::GATEWAY_OP_RESUME: {
			LogInfoEx("Discord Event", "resume");
		} break;

		case Discord::GATEWAY_OP_RECONNECT: {
			LogInfoEx("Discord Event", "reconnect");
		} break;

		case Discord::GATEWAY_OP_REQUEST_GUILD_MEMBERS: {
			LogInfoEx("Discord Event", "guild members");
		} break;

		case Discord::GATEWAY_OP_INVALID_SESSION: {
			LogInfoEx("Discord Event", "invalid session");
		} break;
	}
}

//
//
//

void HandleWebsocketEvent(Websocket *websocket, const Websocket_Event &event, void *user_context) {
	auto arena = ThreadScratchpad();

	auto client = (Discord::Client *)user_context;

	if (event.type == WEBSOCKET_EVENT_TEXT) {
		OnWebsocketMessage(websocket, client, event.message);
		return;
	}

	static uint8_t buffer[4096]; // max allowed discord payload

	if (event.type == WEBSOCKET_EVENT_WRITE) {
		if (client->identify) {

			int length = snprintf((char *)buffer, sizeof(buffer), R"foo(
{
  "op": 2,
  "d": {
    "token": "%s",
    "intents": 513,
    "properties": {
      "$os": "Windows"
    }
  }
}
)foo", client->token);

			auto temp = BeginTemporaryMemory(arena);
			Websocket_SendText(websocket, Buffer(buffer, length));
			EndTemporaryMemory(&temp);

			client->identify = false;

			LogInfoEx("Identify", "Notice me senpai");
		}
	}

	if (event.type == WEBSOCKET_EVENT_TIMEOUT) {
		if (client->hello_received) {
			if (client->heartbeat != client->acknowledgement) {
				// @todo: only wait for certain number of times
				LogInfoEx("Heartbeat", "Senpai failed to notice me");
			}

			int length = 0;
			if (client->s > 0)
				length = snprintf((char *)buffer, sizeof(buffer), "{\"op\": 1, \"d\":%d}", client->s);
			else
				length = snprintf((char *)buffer, sizeof(buffer), "{\"op\": 1, \"d\":null}");
			auto temp = BeginTemporaryMemory(arena);
			Websocket_SendText(websocket, Buffer(buffer, length));
			EndTemporaryMemory(&temp);
			client->heartbeat += 1;
			LogInfoEx("Heartbeat", "Notice me senpai");

			client->imm_heartbeat = false;
		}
	}
}

int main(int argc, char **argv) {
	InitThreadContext(MegaBytes(64));
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	String authorization = FmtStr(ThreadScratchpad(), "Bot %s", argv[1]);

	Http_Response res;
	Websocket_Header headers;
	Websocket_InitHeader(&headers);
	Websocket_HeaderSet(&headers, HTTP_HEADER_AUTHORIZATION, authorization);
	Websocket_HeaderSet(&headers, HTTP_HEADER_USER_AGENT, "Katachi");

	Websocket *websocket = Websocket_Connect("wss://gateway.discord.gg/?v=9&encoding=json", &res, &headers);

	if (!websocket) {
		return 1;
	}

	Discord::Client client;
	client.token = argv[1];

	Websocket_Loop(websocket, HandleWebsocketEvent, &client);
	Net_Shutdown();

	return 0;
}
