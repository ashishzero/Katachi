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

	typedef uint64_t Permission;
	struct PermissionBit {
		enum : uint64_t {
			CREATE_INSTANT_INVITE      = 1ull << 0,
			KICK_MEMBERS               = 1ull << 1,
			BAN_MEMBERS                = 1ull << 2,
			ADMINISTRATOR              = 1ull << 3,
			MANAGE_CHANNELS            = 1ull << 4,
			MANAGE_GUILD               = 1ull << 5,
			ADD_REACTIONS              = 1ull << 6,
			VIEW_AUDIT_LOG             = 1ull << 7,
			PRIORITY_SPEAKER           = 1ull << 8,
			STREAM                     = 1ull << 9,
			VIEW_CHANNEL               = 1ull << 10,
			SEND_MESSAGES              = 1ull << 11,
			SEND_TTS_MESSAGES          = 1ull << 12,
			MANAGE_MESSAGES            = 1ull << 13,
			EMBED_LINKS                = 1ull << 14,
			ATTACH_FILES               = 1ull << 15,
			READ_MESSAGE_HISTORY       = 1ull << 16,
			MENTION_EVERYONE           = 1ull << 17,
			USE_EXTERNAL_EMOJIS        = 1ull << 18,
			VIEW_GUILD_INSIGHTS        = 1ull << 19,
			CONNECT                    = 1ull << 20,
			SPEAK                      = 1ull << 21,
			MUTE_MEMBERS               = 1ull << 22,
			DEAFEN_MEMBERS             = 1ull << 23,
			MOVE_MEMBERS               = 1ull << 24,
			USE_VAD                    = 1ull << 25,
			CHANGE_NICKNAME            = 1ull << 26,
			MANAGE_NICKNAMES           = 1ull << 27,
			MANAGE_ROLES               = 1ull << 28,
			MANAGE_WEBHOOKS            = 1ull << 29,
			MANAGE_EMOJIS_AND_STICKERS = 1ull << 30,
			USE_APPLICATION_COMMANDS   = 1ull << 31,
			REQUEST_TO_SPEAK           = 1ull << 32,
			MANAGE_EVENTS              = 1ull << 33,
			MANAGE_THREADS             = 1ull << 34,
			CREATE_PUBLIC_THREADS      = 1ull << 35,
			CREATE_PRIVATE_THREADS     = 1ull << 36,
			USE_EXTERNAL_STICKERS      = 1ull << 37,
			SEND_MESSAGES_IN_THREADS   = 1ull << 38,
			START_EMBEDDED_ACTIVITIES  = 1ull << 39,
			MODERATE_MEMBERS           = 1ull << 40
		};
	};

	//
	//
	//

	enum class PremiumType {
		NONE          = 0,
		NITRO_CLASSIC = 1,
		NITRO         = 2
	};

	typedef int32_t UserFlag;
	struct UserFlagBit {
		enum : int32_t {
			STAFF                    = 1 << 0,
			PARTNER                  = 1 << 1,
			HYPESQUAD                = 1 << 2,
			BUG_HUNTER_LEVEL_1       = 1 << 3,
			HYPESQUAD_ONLINE_HOUSE_1 = 1 << 6,
			HYPESQUAD_ONLINE_HOUSE_2 = 1 << 7,
			HYPESQUAD_ONLINE_HOUSE_3 = 1 << 8,
			PREMIUM_EARLY_SUPPORTER  = 1 << 9,
			TEAM_PSEUDO_USER         = 1 << 10,
			BUG_HUNTER_LEVEL_2       = 1 << 14,
			VERIFIED_BOT             = 1 << 16,
			VERIFIED_DEVELOPER       = 1 << 17,
			CERTIFIED_MODERATOR      = 1 << 18,
			BOT_HTTP_INTERACTIONS    = 1 << 19,
		};
	};

	struct User {
		Snowflake   id;
		String      username;
		String      discriminator;
		String      avatar;
		bool        bot = false;
		bool        system = false;
		bool        mfa_enabled = false;
		String      banner;
		int32_t     accent_color = 0;
		String      locale;
		bool        verified = false;
		String      email;
		UserFlag    flags = 0;
		PremiumType premium_type = PremiumType::NONE;
		UserFlag    public_flags = 0;
	};

	//
	//
	//

	typedef int32_t ApplicationFlag;
	struct ApplicationFlagBit {
		enum : int32_t {
			GATEWAY_PRESENCE                 = 1 << 12,
			GATEWAY_PRESENCE_LIMITED         = 1 << 13,
			GATEWAY_GUILD_MEMBERS            = 1 << 14,
			GATEWAY_GUILD_MEMBERS_LIMITED    = 1 << 15,
			VERIFICATION_PENDING_GUILD_LIMIT = 1 << 16,
			EMBEDDED                         = 1 << 17,
			GATEWAY_MESSAGE_CONTENT          = 1 << 18,
			GATEWAY_MESSAGE_CONTENT_LIMITED  = 1 << 19,
		};
	};

	struct InstallParams {
		Array<String> scopes;
		Permission    permissions = 0;

		InstallParams() = default;
		InstallParams(Memory_Allocator allocator): scopes(allocator) {}
	};

	struct Application {
		Snowflake       id;
		String          name;
		String          icon;
		String          description;
		Array<String>   rpc_origins;
		bool            bot_public = false;
		bool            bot_require_code_grant = false;
		String          terms_of_service_url;
		String          privacy_policy_url;
		User *          owner = nullptr;
		String          verify_key;
		struct Team *   team = nullptr;
		Snowflake       guild_id;
		Snowflake       primary_sku_id;
		String          slug;
		String          cover_image;
		ApplicationFlag flags = 0;
		String          tags[5];
		InstallParams * install_params = nullptr;
		String          custom_install_url;

		Application() = default;
		Application(Memory_Allocator allocator): rpc_origins(allocator) {}
	};

	//
	//
	//

	enum class Opcode {
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
	};

	//
	//
	//

	struct Intent {
		enum : int {
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
	};

	enum class StatusType { ONLINE, DO_NOT_DISTURB, AFK, INVISIBLE, OFFLINE };

	enum class ActivityType { GAME, STREAMING, LISTENING, WATCHING, CUSTOM, COMPETING };

	typedef int32_t ActivityFlag;
	struct ActivityFlagBit {
		enum : int32_t {
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
		ActivityFlag          flags    = 0;
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

	//
	//
	//

	enum class EventType {
		HELLO, READY, RESUMED, RECONNECT, INVALID_SESSION,
		APPLICATION_COMMAND_PERMISSIONS_UPDATE,
		CHANNEL_CREATE, CHANNEL_UPDATE, CHANNEL_DELETE, CHANNEL_PINS_UPDATE,
		THREAD_CREATE, THREAD_UPDATE, THREAD_DELETE,
		THREAD_LIST_SYNC, THREAD_MEMBER_UPDATE, THREAD_MEMBERS_UPDATE,
		GUILD_CREATE, GUILD_DELETE, GUILD_BAN_ADD, GUILD_BAN_REMOVE,
		GUILD_EMOJIS_UPDATE, GUILD_STICKERS_UPDATE, GUILD_INTEGRATIONS_UPDATE,
		GUILD_MEMBER_ADD, GUILD_MEMBER_REMOVE, GUILD_MEMBER_UPDATE, GUILD_MEMBERS_CHUNK,
		GUILD_ROLE_CREATE, GUILD_ROLE_UPDATE, GUILD_ROLE_DELETE,
		GUILD_SCHEDULED_EVENT_CREATE, GUILD_SCHEDULED_EVENT_UPDATE, GUILD_SCHEDULED_EVENT_DELETE,
		GUILD_SCHEDULED_EVENT_USER_ADD, GUILD_SCHEDULED_EVENT_USER_REMOVE,
		INTEGRATION_CREATE, INTEGRATION_UPDATE, INTEGRATION_DELETE, INTERACTION_CREATE,
		INVITE_CREATE, INVITE_DELETE, MESSAGE_CREATE, MESSAGE_UPDATE, MESSAGE_DELETE,
		MESSAGE_DELETE_BULK, MESSAGE_REACTION_ADD, MESSAGE_REACTION_REMOVE,
		MESSAGE_REACTION_REMOVE_ALL, MESSAGE_REACTION_REMOVE_EMOJI,
		PRESENCE_UPDATE, STAGE_INSTANCE_CREATE, STAGE_INSTANCE_DELETE, STAGE_INSTANCE_UPDATE,
		TYPING_START, USER_UPDATE, VOICE_STATE_UPDATE, VOICE_SERVER_UPDATE, WEBHOOKS_UPDATE,

		EVENT_COUNT
	};

	static const String EventNames[] = {
		"HELLO", "READY", "RESUMED", "RECONNECT", "INVALID_SESSION",
		"APPLICATION_COMMAND_PERMISSIONS_UPDATE",
		"CHANNEL_CREATE", "CHANNEL_UPDATE", "CHANNEL_DELETE", "CHANNEL_PINS_UPDATE",
		"THREAD_CREATE", "THREAD_UPDATE", "THREAD_DELETE",
		"THREAD_LIST_SYNC", "THREAD_MEMBER_UPDATE", "THREAD_MEMBERS_UPDATE",
		"GUILD_CREATE", "GUILD_DELETE", "GUILD_BAN_ADD", "GUILD_BAN_REMOVE",
		"GUILD_EMOJIS_UPDATE", "GUILD_STICKERS_UPDATE", "GUILD_INTEGRATIONS_UPDATE",
		"GUILD_MEMBER_ADD", "GUILD_MEMBER_REMOVE", "GUILD_MEMBER_UPDATE", "GUILD_MEMBERS_CHUNK",
		"GUILD_ROLE_CREATE", "GUILD_ROLE_UPDATE", "GUILD_ROLE_DELETE",
		"GUILD_SCHEDULED_EVENT_CREATE", "GUILD_SCHEDULED_EVENT_UPDATE", "GUILD_SCHEDULED_EVENT_DELETE",
		"GUILD_SCHEDULED_EVENT_USER_ADD", "GUILD_SCHEDULED_EVENT_USER_REMOVE",
		"INTEGRATION_CREATE", "INTEGRATION_UPDATE", "INTEGRATION_DELETE", "INTERACTION_CREATE",
		"INVITE_CREATE", "INVITE_DELETE", "MESSAGE_CREATE", "MESSAGE_UPDATE", "MESSAGE_DELETE",
		"MESSAGE_DELETE_BULK", "MESSAGE_REACTION_ADD", "MESSAGE_REACTION_REMOVE",
		"MESSAGE_REACTION_REMOVE_ALL", "MESSAGE_REACTION_REMOVE_EMOJI",
		"PRESENCE_UPDATE", "STAGE_INSTANCE_CREATE", "STAGE_INSTANCE_DELETE", "STAGE_INSTANCE_UPDATE",
		"TYPING_START", "USER_UPDATE", "VOICE_STATE_UPDATE", "VOICE_SERVER_UPDATE", "WEBHOOKS_UPDATE",
	};

	static_assert(ArrayCount(EventNames) == (int)EventType::EVENT_COUNT, "");

	struct Event {
		EventType type;
	};

	struct Hello : public Event {
		int32_t heartbeat_interval;
	};

	struct Ready : public Event {
		int32_t          v;
		User             user;
		Array<Snowflake> guilds;
		String           session_id;
		int32_t          shard[2] = {};
		Application      application;

		Ready() = default;
		Ready(Memory_Allocator allocator): guilds(allocator), application(allocator) {}
	};

	struct Resumed : public Event {};
	struct Reconnect : public Event {};

	struct InvalidSession : public Event {
		bool resumable = false;
	};

	//
	//
	//

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
		j.KeyValue("op", (int)Discord::Opcode::IDENTIFY);
		j.PushKey("d");
		Discord_Jsonify(client->identify, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendResume(Client *client) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::Opcode::RESUME);
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
		j.KeyValue("op", (int)Discord::Opcode::HEARTBEAT);
		if (client->sequence >= 0)
			j.KeyValue("d", client->sequence);
		else
			j.KeyNull("d");
		j.EndObject();

		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
		client->heartbeat.count += 1;
	}

	void SendGuildMembersRequest(Client *client, const GuildMembersRequest &req_guild_mems) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::Opcode::REQUEST_GUILD_MEMBERS);
		j.PushKey("d");
		Discord_Jsonify(req_guild_mems, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendVoiceStateUpdate(Client *client, const VoiceStateUpdate &update_voice_state) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::Opcode::UPDATE_VOICE_STATE);
		j.PushKey("d");
		Discord_Jsonify(update_voice_state, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendPresenceUpdate(Client *client, const PresenceUpdate &presence_update) {
		Jsonify j(client->arena);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::Opcode::UPDATE_PRESECE);
		j.PushKey("d");
		Discord_Jsonify(presence_update, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}
}

static void Discord_HandleEvent(Discord::Client *client, String event, const Json_Object &data) {
	for (int index = 0; index < ArrayCount(Discord::EventNames); ++index) {
		if (event == Discord::EventNames[index]) {
			return;
		}
	}
	LogErrorEx("Discord", "Unknown event: " StrFmt, event);
}

static void Discord_HandleWebsocketEvent(Discord::Client *client, const Websocket_Event &event) {
	if (event.type != WEBSOCKET_EVENT_TEXT)
		return;

	Json json;
	if (JsonParse(event.message, &json, MemoryArenaAllocator(client->arena))) {
		Json_Object payload = JsonGetObject(json);
		int         opcode  = JsonGetInt(payload, "op");
		Json        data    = JsonGet(payload, "d");

		if (opcode == (int)Discord::Opcode::DISPATH) {
			client->sequence  = JsonGetInt(payload, "s", client->sequence);
			String event_name = JsonGetString(payload, "t");

			TraceEx("Discord", "Event: " StrFmt, StrArg(event_name));
			Discord_HandleEvent(client, event_name, JsonGetObject(data));
			return;
		}

		if (opcode == (int)Discord::Opcode::HEARTBEAT) {
			TraceEx("Discord", "Heartbeat (%d)", client->heartbeat.count);
			Discord::SendHearbeat(client);
			return;
		}

		if (opcode == (int)Discord::Opcode::RECONNECT) {
			Unimplemented();
			TraceEx("Discord", "Reconnect");
			return;
		}

		if (opcode == (int)Discord::Opcode::INVALID_SESSION) {
			Unimplemented();
			TraceEx("Discord", "Invalid Session");
			return;
		}

		if (opcode == (int)Discord::Opcode::HELLO) {
			TraceEx("Discord", "Hello");
			Json_Object obj            = JsonGetObject(data);
			client->heartbeat.interval = JsonGetFloat(obj, "heartbeat_interval", 45000);
			Discord::SendIdentify(client, client->identify);
			return;
		}

		if (opcode == (int)Discord::Opcode::HEARTBEAT_ACK) {
			client->heartbeat.acknowledged += 1;
			TraceEx("Discord", "Acknowledgement (%d)", client->heartbeat.acknowledged);
			return;
		}

		Unreachable();
		return;
	}

	LogErrorEx("Discord", "Invalid Frame received: " StrFmt, StrArg(event.message));
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
	TraceEx("Discord", "Getway URL: " StrFmt ", Shards: %d", StrArg(url), shards);

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
	intents |= Discord::Intent::GUILDS;
	intents |= Discord::Intent::GUILD_MEMBERS;
	intents |= Discord::Intent::GUILD_MESSAGES;
	intents |= Discord::Intent::DIRECT_MESSAGE_REACTIONS;

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

		if (res == WEBSOCKET_E_WAIT) {
			Discord::SendHearbeat(&client);
			TraceEx("Discord", "Heartbeat (%d)", client.heartbeat.count);
		}
	}

	Websocket_Close(websocket, WEBSOCKET_CLOSE_GOING_AWAY);

	Net_Shutdown();

	return 0;
}
