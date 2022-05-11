#include "Kr/KrBasic.h"
#include "Network.h"
#include "Kr/KrString.h"
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
	struct Snowflake { uint64_t value = 0; };

	enum GatewayOpcode {
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

	enum GatewayIntent {
		GUILDS                    = 1 << 0,
		GUILD_MEMBERS             = 1 << 1,
		GUILD_BANS                = 1 << 2,
		GUILD_EMOJIS_AND_STICKERS = 1 << 3,
		GUILD_INTEGRATIONS        = 1 << 4,
		GUILD_WEBHOOKS            = 1 << 5,
		GUILD_INVITES             = 1 << 6,
		GUILD_VOICE_STATES        = 1 << 7,
		GUILD_PRESENCES           = 1 << 8,
		GUILD_MESSAGES            = 1 << 9,
		GUILD_MESSAGE_REACTIONS   = 1 << 10,
		GUILD_MESSAGE_TYPING      = 1 << 11,
		DIRECT_MESSAGES           = 1 << 12,
		DIRECT_MESSAGE_REACTIONS  = 1 << 13,
		DIRECT_MESSAGE_TYPING     = 1 << 14,
		GUILD_SCHEDULED_EVENTS    = 1 << 15,
	};

	enum class StatusType { ONLINE, DO_NOT_DISTURB, AFK, INVISIBLE, OFFLINE };

	enum class ActivityType { GAME, STREAMING, LISTENING, WATCHING, CUSTOM, COMPETING };

	enum class ActivityFlag {
		INSTANCE                    = 1 << 0,
		JOIN                        = 1 << 1,
		SPECTATE                    = 1 << 2,
		JOIN_REQUEST                = 1 << 3,
		SYNC                        = 1 << 4,
		PLAY                        = 1 << 5,
		PARTY_PRIVACY_FRIENDS       = 1 << 6,
		PARTY_PRIVACY_VOICE_CHANNEL = 1 << 7,
		EMBEDDED                    = 1 << 8
	};

	struct ActivityTimestamps {
		int32_t start = 0;
		int32_t end   = 0;
	};

	struct ActivityEmoji {
		String    name;
		Snowflake id;
		bool      animated = false;
	};

	struct ActivityParty {
		String  id;
		int32_t size[2] = {0,0};
	};

	struct ActivityAssets {
		String large_image;
		String large_text;
		String small_image;
		String small_text;
	};

	struct ActivitySecrets {
		String join;
		String spectate;
		String match;
	};

	struct ActivityButton {
		String label;
		String url;
	};

	struct Activity {
		String                name;
		ActivityType          type = ActivityType::GAME;
		String                url;
		int32_t               created_at = 0;
		ActivityTimestamps    timestamps;
		Snowflake             application_id;
		String                details;
		String                state;
		ActivityEmoji *       emoji    = nullptr;
		ActivityParty *       party    = nullptr;
		ActivityAssets *      assets   = nullptr;
		ActivitySecrets *     secrets  = nullptr;
		bool                  instance = false;
		int32_t               flags    = 0;
		Array<ActivityButton> buttons;

		Activity() = default;
		Activity(Memory_Allocator allocator): buttons(allocator) {}
	};

	struct PresenceUpdate {
		int32_t         since  = 0;
		Array<Activity> activities;
		StatusType      status = StatusType::ONLINE;
		bool            afk    = false;

		PresenceUpdate() = default;
		PresenceUpdate(Memory_Allocator allocator): activities(allocator) {}
	};

	struct Identify {
		String          token;
		struct {
			String      os = __PLATFORM__;
			String      browser;
			String      device;
		}               properties;
		bool            compress = false;
		int32_t         large_threshold = 0;
		int32_t         shard[2] = {0, 1};
		PresenceUpdate *presence = nullptr;
		int32_t         intents  = 0;

		Identify() = default;
		Identify(String _token, int32_t _intents, PresenceUpdate *_presence = nullptr) :
			token(_token), presence(_presence), intents(_intents) {}
	};

	enum Client_State {
		CLIENT_CONNECTING,
		CLIENT_CONNECTED,

		_CLIENT_STATE_COUNT
	};

	struct Client {
		Client_State state = CLIENT_CONNECTING;

		int heartbeat_time   = 2000;
		int heartbeat_rem    = 0;
		int heartbeats       = 0;
		int acknowledgements = 0;
		int sequence         = -1;

		String token;
		String authorization;

		Memory_Arena *arena = nullptr;
	};
}

static void Discord_Jsonify(const Discord::Activity &activity, Jsonify *j) {
	j->BeginObject();
	j->KeyValue("name", activity.name);
	j->KeyValue("type", (int)activity.type);
	if (activity.url.length)
		j->KeyValue("url", activity.url);
	j->EndObject();
}

static void Discord_Jsonify(const Discord::PresenceUpdate &presence, Jsonify *j) {
	j->BeginObject();
	if (presence.since)
		j->KeyValue("since", presence.since);

	j->PushKey("activities");
	j->BeginArray();
	for (const auto &activity : presence.activities)
		Discord_Jsonify(activity, j);
	j->EndArray();

	j->PushKey("status");
	switch (presence.status) {
		case Discord::StatusType::ONLINE:         j->PushString("online"); break;
		case Discord::StatusType::DO_NOT_DISTURB: j->PushString("dnd"); break;
		case Discord::StatusType::AFK:            j->PushString("idle"); break;
		case Discord::StatusType::INVISIBLE:      j->PushString("invisible"); break;
		case Discord::StatusType::OFFLINE:        j->PushString("offline"); break;
		NoDefaultCase();
	}

	j->KeyValue("afk", presence.afk);
	j->EndObject();
}

static void Discord_Jsonify(const Discord::Identify &identify, Jsonify *j) {
	j->BeginObject();

	j->KeyValue("op", Discord::GATEWAY_OP_IDENTIFY);
	j->PushKey("d");
	j->BeginObject();

	j->KeyValue("token", identify.token);
	j->PushKey("properties");
	j->BeginObject();
	if (identify.properties.os.length) j->KeyValue("$os", identify.properties.os);
	if (identify.properties.browser.length) j->KeyValue("$browser", identify.properties.browser);
	if (identify.properties.device.length) j->KeyValue("$device", identify.properties.device);
	j->EndObject();

	j->KeyValue("compress", identify.compress);
	if (identify.large_threshold >= 50 && identify.large_threshold <= 250)
		j->KeyValue("large_threshold", identify.large_threshold);

	if (identify.shard[1]) {
		j->PushKey("shard");
		j->BeginArray();
		j->PushInt(identify.shard[0]);
		j->PushInt(identify.shard[1]);
		j->EndArray();
	}

	if (identify.presence) {
		j->PushKey("presence");
		Discord_Jsonify(*identify.presence, j);
	}

	j->KeyValue("intents", identify.intents);

	j->EndObject();

	j->EndObject();
}

static void Discord_Hearbeat(Websocket *websocket, Discord::Client *client) {
	if (client->heartbeats != client->acknowledgements) {
		// @todo: only wait for certain number of times
		LogWarningEx("Discord", "Acknowledgement not reveived (%d/%d)", client->heartbeats, client->acknowledgements);
	}

	Jsonify j(client->arena);
	j.BeginObject();
	j.KeyValue("op", Discord::GATEWAY_OP_HEARTBEAT);
	if (client->sequence >= 0)
		j.KeyValue("d", client->sequence);
	else
		j.KeyNull("d");
	j.EndObject();

	String msg = Jsonify_BuildString(&j);
	Websocket_SendText(websocket, msg, client->heartbeat_rem);
	client->heartbeats += 1;

	Trace("Heartbeat (%d)", client->heartbeats);
}

static void Discord_HandleWebsocketEvent(Websocket *websocket, const Websocket_Event &event, Discord::Client *client) {
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
					Trace("Hello");

					Json_Object d          = JsonGetObject(obj, "d");
					int heartbeat_interval = JsonGetInt(d, "heartbeat_interval", 45000);
					client->heartbeat_time = heartbeat_interval;

					// @todo: don't got the connected state yet wait for "ready" event from discord
					// reconnect or disconnect is "ready" event is not present within some time limit
					client->state = Discord::CLIENT_CONNECTED;

					int intents = 0;
					intents |= Discord::GatewayIntent::DIRECT_MESSAGES;
					intents |= Discord::GatewayIntent::GUILD_MESSAGES;
					intents |= Discord::GatewayIntent::GUILD_MESSAGE_REACTIONS;

					Memory_Allocator allocator = MemoryArenaAllocator(client->arena);

					Discord::PresenceUpdate presence(allocator);
					presence.status = Discord::StatusType::DO_NOT_DISTURB;
					Discord::Activity *activity = presence.activities.Add();
					activity->name = "Spotify";
					activity->type = Discord::ActivityType::LISTENING;
					activity->url  = "https://open.spotify.com/track/0ZiO07cHvb675UDaKB1iix?si=7e198a75a163493c";

					Discord::Identify identify(client->token, intents, &presence);

					Jsonify j(client->arena);
					Discord_Jsonify(identify, &j);
					String msg = Jsonify_BuildString(&j);

					// @note:
					// @reference: https://discord.com/developers/docs/topics/gateway#identifying-example-gateway-identify
					// Clients are limited to 1000 IDENTIFY calls to the websocket in a 24-hour period.
					// This limit is global and across all shards, but does not include RESUME calls.
					// Upon hitting this limit, all active sessions for the bot will be terminated,
					// the bot's token will be reset, and the owner will receive an email notification.
					// It's up to the owner to update their application with the new token.

					Websocket_SendText(websocket, msg, client->heartbeat_rem);
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

	ptrdiff_t buff_len = KiloBytes(16);

	Websocket_Event event = {};
	event.message.data = PushArray(client.arena, uint8_t, buff_len);

	clock_t counter = clock();

	client.heartbeat_rem = client.heartbeat_time;

	while (Websocket_IsConnected(websocket)) {
		Websocket_Result res = Websocket_Receive(websocket, &event, buff_len, client.heartbeat_rem);

		if (res == WEBSOCKET_OK) {
			Discord_HandleWebsocketEvent(websocket, event, &client);
		}

		if (res == WEBSOCKET_E_CLOSED) break;

		clock_t new_counter = clock();
		float spent = (1000.0f * (new_counter - counter)) / CLOCKS_PER_SEC;
		client.heartbeat_rem -= spent;
		counter = new_counter;

		if (client.heartbeat_rem <= 0) {
			client.heartbeat_rem = client.heartbeat_time;
			res = WEBSOCKET_E_WAIT;
		}

		if (res == WEBSOCKET_E_WAIT) {
			Discord_Hearbeat(websocket, &client);
		}
	}

	Net_Shutdown();

	return 0;
}
