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
	const String UserAgent = "Katachi (https://github.com/Zero5620/Katachi, 0.1.1)";

	struct Snowflake { uint64_t value = 0; };

	enum class GatewayOpcode {
		DISPATH               = 0,
		HEARTBEAT             = 1,
		IDENTIFY              = 2,
		UPDATE_PRESECE        = 3,
		UPDATE_VOICE_STATE    = 4,
		RESUME                = 6,
		RECONNECT             = 7,
		REQUEST_GUILD_MEMBERS = 8,
		INVALID_SESSION       = 9,
		HELLO                 = 10,
		HEARTBEAT_ACK         = 11,

		_COUNT_
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

	struct GuildMembersRequest {
		Snowflake        guild_id;
		String           query;
		int32_t          limit     = 0;
		bool             presences = false;
		Array<Snowflake> user_ids;
		String           nonce;

		GuildMembersRequest() = default;
		GuildMembersRequest(Memory_Allocator _allocator): user_ids(_allocator){}
	};

	struct VoiceStateUpdate {
		Snowflake guild_id;
		Snowflake channel_id;
		bool      self_mute = false;
		bool      self_deaf = false;
	};

	struct Heartbeat {
		float interval     = 2000.0f;
		float remaining    = 0.0f;
		int   count        = 0;
		int   acknowledged = 0;
	};

	struct Client {
		Websocket *   websocket;
		Identify      identify; // @todo: @remove
		Heartbeat     heartbeat;

		String        token;
		String        session_id;
		int           sequence = -1;

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
}

static void Discord_Jsonify(const Discord::GuildMembersRequest &req_guild_mems, Jsonify *j) {
	j->BeginObject();
	j->KeyValue("guild_id", req_guild_mems.guild_id.value);
	j->KeyValue("limit", req_guild_mems.limit);
	j->KeyValue("presences", req_guild_mems.presences);
	if (req_guild_mems.user_ids.count) {
		j->PushKey("user_ids");
		j->BeginArray();
		for (Discord::Snowflake id : req_guild_mems.user_ids)
			j->PushId(id.value);
		j->EndArray();
	} else {
		j->KeyValue("query", req_guild_mems.query);
	}
	if (req_guild_mems.nonce.length)
		j->KeyValue("nonce", req_guild_mems.nonce);
	j->EndObject();
}

static void Discord_Jsonify(const Discord::VoiceStateUpdate &update_voice_state, Jsonify *j) {
	j->BeginObject();
	j->KeyValue("guild_id", update_voice_state.guild_id.value);
	if (update_voice_state.channel_id.value)
		j->KeyValue("channel_id", update_voice_state.channel_id.value);
	else
		j->KeyNull("channel_id");
	j->KeyValue("self_mute", update_voice_state.self_mute);
	j->KeyValue("self_deaf", update_voice_state.self_deaf);
	j->EndObject();
}

namespace Discord {
	void SendIdentify(Client *client, const Identify &identify) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::GatewayOpcode::IDENTIFY);
		j.PushKey("d");
		Discord_Jsonify(client->identify, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendResume(Client *client) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::GatewayOpcode::RESUME);
		j.PushKey("d");
		j.BeginObject();
		j.KeyValue("token", client->token);
		j.KeyValue("session_id", client->session_id);
		if (client->sequence >= 0)
			j.KeyValue("seq", client->sequence);
		else
			j.KeyNull("seq");
		j.EndObject();
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendHearbeat(Client *client) {
		if (client->heartbeat.count != client->heartbeat.acknowledged) {
			// @todo: only wait for certain number of times
			LogWarningEx("Discord", "Acknowledgement not reveived (%d/%d)", client->heartbeat.count, client->heartbeat.acknowledged);
		}

		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::GatewayOpcode::HEARTBEAT);
		if (client->sequence >= 0)
			j.KeyValue("d", client->sequence);
		else
			j.KeyNull("d");
		j.EndObject();

		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
		client->heartbeat.count += 1;

		Trace("Heartbeat (%d)", client->heartbeat.count);
	}

	void SendGuildMembersRequest(Client *client, const GuildMembersRequest &req_guild_mems) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::GatewayOpcode::REQUEST_GUILD_MEMBERS);
		j.PushKey("d");
		Discord_Jsonify(req_guild_mems, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendVoiceStateUpdate(Client *client, const VoiceStateUpdate &update_voice_state) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::GatewayOpcode::UPDATE_VOICE_STATE);
		j.PushKey("d");
		Discord_Jsonify(update_voice_state, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendPresenceUpdate(Client *client, const PresenceUpdate &presence_update) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::GatewayOpcode::UPDATE_PRESECE);
		j.PushKey("d");
		Discord_Jsonify(presence_update, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}
}

typedef void(*Discord_Gateway_Handler)(Websocket *websocket, const Json &data, Discord::Client *client);

static void Discord_GatewayDispatch(Websocket *websocket, const Json &data, Discord::Client *client) {
}

static void Discord_GatewayHeartbeat(Websocket *websocket, const Json &data, Discord::Client *client) {
	Discord::SendHearbeat(client);
}

static void Discord_GatewayReconnect(Websocket *websocket, const Json &data, Discord::Client *client) {
}

static void Discord_GatewayInvalidSession(Websocket *websocket, const Json &data, Discord::Client *client) {
}

static void Discord_GatewayHello(Websocket *websocket, const Json &data, Discord::Client *client) {
	Json_Object obj = JsonGetObject(data);

	client->heartbeat.interval = JsonGetFloat(obj, "heartbeat_interval", 45000);

	Discord::SendIdentify(client, client->identify);
}

static void Discord_GatewayHeartbeatAck(Websocket *websocket, const Json &data, Discord::Client *client) {
	client->heartbeat.acknowledged += 1;
	Trace("Acknowledgement (%d)", client->heartbeat.acknowledged);
}

static void Discord_GatewayUnreachable(Websocket *websocket, const Json &data, Discord::Client *client) {
	Unreachable();
}

static constexpr Discord_Gateway_Handler DiscordGatewayHandlers[] = {
		Discord_GatewayDispatch,            // 0
		Discord_GatewayHeartbeat,           // 1
		Discord_GatewayUnreachable,         // 2
		Discord_GatewayUnreachable,         // 3
		Discord_GatewayUnreachable,         // 4
		Discord_GatewayUnreachable,         // 5
		Discord_GatewayUnreachable,         // 6
		Discord_GatewayReconnect,           // 7
		Discord_GatewayUnreachable,         // 8
		Discord_GatewayInvalidSession,      // 9
		Discord_GatewayHello,               // 10
		Discord_GatewayHeartbeatAck         // 11
};
static_assert((int)Discord::GatewayOpcode::_COUNT_ == ArrayCount(DiscordGatewayHandlers), "");

static void Discord_HandleWebsocketEvent(Discord::Client *client, const Websocket_Event &event) {
	if (event.type != WEBSOCKET_EVENT_TEXT)
		return;

	Json json;
	if (JsonParse(event.message, &json, MemoryArenaAllocator(client->arena))) {
		Json_Object payload = JsonGetObject(json);
		int opcode          = JsonGetInt(payload, "op");
		Json        data    = JsonGet(payload, "d");
		client->sequence    = JsonGetInt(payload, "s", client->sequence);
		String event_name   = JsonGetString(payload, "t");

		Trace("Discord Gateway; Opcode: %d, " StrFmt, opcode, StrArg(event_name));

		DiscordGatewayHandlers[(int)opcode](client->websocket, data, client);

		return;
	}

	LogError("Invalid Event received: " StrFmt, StrArg(event.message));
}

// @todo
// Resume: https://discord.com/developers/docs/topics/gateway#resuming
// Disconnect: https://discord.com/developers/docs/topics/gateway#disconnections
// Sharding: https://discord.com/developers/docs/topics/gateway#sharding
// Commands: https://discord.com/developers/docs/topics/gateway#commands-and-events-gateway-commands

int main(int argc, char **argv) {
	InitThreadContext(KiloBytes(64));
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	Discord::Client client;
	client.arena = MemoryArenaAllocate(MegaBytes(64));

	if (!client.arena) {
		return 1; // @todo log error
	}

	client.token         = FmtStr(client.arena, "%s", argv[1]);
	String authorization = FmtStr(ThreadScratchpad(), "Bot %s", argv[1]);

	Http *http = Http_Connect("https://discord.com");
	if (!http) {
		return 1;
	}

	auto temp = BeginTemporaryMemory(client.arena);

	Http_Request req;
	Http_InitRequest(&req);
	Http_SetHost(&req, http);
	Http_SetHeader(&req, HTTP_HEADER_AUTHORIZATION, authorization);
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
	Websocket_HeaderSet(&headers, HTTP_HEADER_AUTHORIZATION, authorization);
	Websocket_HeaderSet(&headers, HTTP_HEADER_USER_AGENT, Discord::UserAgent);

	Websocket_Spec spec;
	spec.queue_size = 16;
	spec.read_size  = MegaBytes(16);
	spec.write_size = KiloBytes(12);

	Websocket *websocket = Websocket_Connect(url, &res, &headers, spec);
	if (!websocket) {
		return 1;
	}

	EndTemporaryMemory(&temp);

	int intents = 0;
	intents |= Discord::GatewayIntent::GUILDS;
	intents |= Discord::GatewayIntent::GUILD_MEMBERS;
	intents |= Discord::GatewayIntent::GUILD_MESSAGES;
	intents |= Discord::GatewayIntent::DIRECT_MESSAGE_REACTIONS;

	Discord::PresenceUpdate presence(MemoryArenaAllocator(client.arena));
	presence.status = Discord::StatusType::DO_NOT_DISTURB;
	auto activity   = presence.activities.Add();
	activity->name  = "Twitch";
	activity->url   = "https://www.twitch.tv/ashishzero";
	activity->type  = Discord::ActivityType::STREAMING;

	client.websocket = websocket;
	client.identify  = Discord::Identify(client.token, intents, &presence);

	clock_t counter = clock();
	client.heartbeat.remaining = client.heartbeat.interval;

	while (Websocket_IsConnected(websocket)) {
		auto temp = BeginTemporaryMemory(client.arena);
		Defer{ EndTemporaryMemory(&temp); };

		Websocket_Event event;
		Websocket_Result res = Websocket_Receive(websocket, &event, client.arena, (int)client.heartbeat.remaining);

		if (res == WEBSOCKET_E_CLOSED) break;

		if (res == WEBSOCKET_OK) {
			Discord_HandleWebsocketEvent(&client, event);
		} else if (res == WEBSOCKET_E_NOMEM) {
			LogWarningEx("Discord", "Packet lost. Reason: Buffer size too small");
		}

		clock_t new_counter = clock();
		float spent = (1000.0f * (new_counter - counter)) / CLOCKS_PER_SEC;
		client.heartbeat.remaining -= spent;
		counter = new_counter;

		if (client.heartbeat.remaining <= 0) {
			client.heartbeat.remaining = client.heartbeat.interval;
			res = WEBSOCKET_E_WAIT;
		}

		if (res == WEBSOCKET_E_WAIT)
			Discord::SendHearbeat(&client);
	}

	Net_Shutdown();

	return 0;
}
