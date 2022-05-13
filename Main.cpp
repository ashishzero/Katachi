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

	struct Snowflake {
		uint64_t value = 0;
		Snowflake() = default;
		Snowflake(uint64_t _val): value(_val) {}
	};

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

	enum class OverwriteType {
		ROLE = 0, MEMBER = 1
	};

	struct Overwrite {
		Snowflake     id;
		OverwriteType type  = OverwriteType::ROLE;
		Permission    allow = 0;
		Permission    deny  = 0;
	};

	//
	//
	//

	enum class ApplicationCommandPermissionType {
		ROLE = 1, USER = 2, CHANNEL = 3
	};

	struct ApplicationCommandPermission {
		Snowflake                        id;
		ApplicationCommandPermissionType type;
		bool                             permission = false;
	};

	struct ApplicationCommandPermissions {
		Snowflake                           id;
		Snowflake                           application_id;
		Snowflake                           guild_id;
		Array<ApplicationCommandPermission> permissions;

		ApplicationCommandPermissions() = default;
		ApplicationCommandPermissions(Memory_Allocator allocator): permissions(allocator) {}
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
		ptrdiff_t             created_at = 0;
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

	//
	//
	//

	struct GuildMember {
		User *           user = nullptr;
		String           nick;
		String           avatar;
		Array<Snowflake> roles;
		ptrdiff_t        joined_at = 0;
		ptrdiff_t        premium_since = 0;
		bool             deaf = false;
		bool             mute = false;
		bool             pending = false;
		Permission       permissions = 0;
		ptrdiff_t        communication_disabled_until = 0;

		GuildMember() = default;
		GuildMember(Memory_Allocator allocator): roles(allocator) {}
	};

	struct ClientStatus {
		String desktop;
		String mobile;
		String web;
	};

	struct Presence {
		User            user;
		Snowflake       guild_id;
		StatusType      status = StatusType::ONLINE;
		Array<Activity> activities;
		ClientStatus    client_status;

		Presence() = default;
		Presence(Memory_Allocator allocator): activities(allocator) {}
	};

	//
	//
	//

	enum class ChannelType {
		GUILD_TEXT           = 0,
		DM                   = 1,
		GUILD_VOICE          = 2,
		GROUP_DM             = 3,
		GUILD_CATEGORY       = 4,
		GUILD_NEWS           = 5,
		GUILD_STORE          = 6,
		GUILD_NEWS_THREAD    = 10,
		GUILD_PUBLIC_THREAD  = 11,
		GUILD_PRIVATE_THREAD = 12,
		GUILD_STAGE_VOICE    = 13,
		GUILD_DIRECTORY      = 14,
		GUILD_FORUM          = 15,
	};

	typedef int32_t ChannelFlag;
	struct ChannelFlagBit {
		enum {
			PINNED = 1 << 1
		};
	};

	struct ThreadMetadata {
		bool      archived = false;
		int32_t   auto_archive_duration = 0;
		ptrdiff_t archive_timestamp = 0;
		bool      locked = false;
		bool      invitable = false;
		ptrdiff_t create_timestamp = 0;
	};

	struct ThreadMember {
		Snowflake      id;
		Snowflake      user_id;
		ptrdiff_t      join_timestamp = 0;
		int32_t        flags          = 0;
		GuildMember *  member        = nullptr;
		Presence *     presence      = nullptr;
	};

	struct Channel {
		Snowflake        id;
		ChannelType      type = ChannelType::GUILD_TEXT;
		Snowflake        guild_id;
		int32_t          position = 0;
		Array<Overwrite> permission_overwrites;
		String           name;
		String           topic;
		bool             nsfw = false;
		Snowflake        last_message_id;
		int32_t          bitrate;
		int32_t          user_limit;
		int32_t          rate_limit_per_user;
		Array<User>      recipients;
		String           icon;
		Snowflake        owner_id;
		Snowflake        application_id;
		Snowflake        parent_id;
		ptrdiff_t        last_pin_timestamp = 0;
		String           rtc_region;
		int32_t          video_quality_mode = 0;
		int32_t          message_count = 0;
		int32_t          member_count = 0;
		ThreadMetadata * thread_metadata;
		ThreadMember *   member;
		int32_t          default_auto_archive_duration = 0;
		Permission       permissions = 0;
		ChannelFlag      flags = 0;

		Channel() = default;
		Channel(Memory_Allocator allocator): permission_overwrites(allocator), recipients(allocator) {}
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
		NONE,
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
		"NONE",
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
		EventType type = EventType::NONE;
		String    name = "NONE";

		Event() = default;
		Event(EventType _type): type(_type), name(EventNames[(int)_type]) {}
	};

	struct HelloEvent : public Event {
		int32_t heartbeat_interval;
		HelloEvent(): Event(EventType::HELLO) {}
	};

	struct ReadyEvent : public Event {
		int32_t          v;
		User             user;
		Array<Snowflake> guilds;
		String           session_id;
		int32_t          shard[2] = {};
		Application      application;

		ReadyEvent() : Event(EventType::READY) {}
		ReadyEvent(Memory_Allocator allocator): Event(EventType::READY), guilds(allocator), application(allocator) {}
	};

	struct ResumedEvent : public Event {
		ResumedEvent(): Event(EventType::RESUMED) {}
	};

	struct ReconnectEvent : public Event {
		ReconnectEvent() : Event(EventType::RECONNECT) {}
	};

	struct InvalidSessionEvent : public Event {
		bool resumable = false;
		InvalidSessionEvent(): Event(EventType::INVALID_SESSION) {}
	};

	struct ApplicationCommandPermissionsUpdateEvent : public Event {
		ApplicationCommandPermissions permissions;

		ApplicationCommandPermissionsUpdateEvent(): Event(EventType::APPLICATION_COMMAND_PERMISSIONS_UPDATE) {}
		ApplicationCommandPermissionsUpdateEvent(Memory_Allocator allocator):
			Event(EventType::APPLICATION_COMMAND_PERMISSIONS_UPDATE), permissions(allocator) {}
	};

	struct ChannelCreateEvent : public Event {
		Channel channel;
		ChannelCreateEvent() : Event(EventType::CHANNEL_CREATE) {}
		ChannelCreateEvent(Memory_Allocator allocator): Event(EventType::CHANNEL_CREATE), channel(allocator) {}
	};

	struct ChannelUpdateEvent : public Event {
		Channel channel;
		ChannelUpdateEvent() : Event(EventType::CHANNEL_UPDATE) {}
		ChannelUpdateEvent(Memory_Allocator allocator) : Event(EventType::CHANNEL_UPDATE), channel(allocator) {}
	};

	struct ChannelDeleteEvent : public Event {
		Channel channel;
		ChannelDeleteEvent(): Event(EventType::CHANNEL_DELETE) {}
		ChannelDeleteEvent(Memory_Allocator allocator) : Event(EventType::CHANNEL_DELETE), channel(allocator) {}
	};

	struct ChannelPinsUpdateEvent : public Event {
		Snowflake guild_id;
		Snowflake channel_id;
		ptrdiff_t last_pin_timestamp;

		ChannelPinsUpdateEvent(): Event(EventType::CHANNEL_PINS_UPDATE) {}
	};

	struct ThreadCreateEvent : public Event {
		Channel channel;
		bool newly_created = false;

		ThreadCreateEvent(): Event(EventType::THREAD_CREATE) {}
		ThreadCreateEvent(Memory_Allocator allocator): Event(EventType::THREAD_CREATE), channel(allocator) {}
	};

	struct ThreadUpdateEvent : public Event {
		Channel channel;
		ThreadUpdateEvent(): Event(EventType::THREAD_UPDATE) {}
		ThreadUpdateEvent(Memory_Allocator allocator) : Event(EventType::THREAD_UPDATE), channel(allocator) {}
	};

	struct ThreadDeleteEvent : public Event {
		Snowflake   id;
		Snowflake   guild_id;
		Snowflake   parent_id;
		ChannelType type = ChannelType::GUILD_TEXT;

		ThreadDeleteEvent(): Event(EventType::THREAD_DELETE) {}
	};

	struct ThreadListSyncEvent : public Event {
		Snowflake           guild_id;
		Array<Snowflake>    channel_ids;
		Array<Channel>      threads;
		Array<ThreadMember> members;

		ThreadListSyncEvent(): Event(EventType::THREAD_LIST_SYNC) {}
		ThreadListSyncEvent(Memory_Allocator allocator): 
			Event(EventType::THREAD_LIST_SYNC), channel_ids(allocator), threads(allocator), members(allocator) {}
	};

	struct ThreadMemberUpdateEvent : public Event {
		ThreadMember member;
		Snowflake    guild_id;

		ThreadMemberUpdateEvent(): Event(EventType::THREAD_MEMBER_UPDATE) {}
	};

	struct ThreadMembersUpdateEvent : public Event {
		Snowflake           id;
		Snowflake           guild_id;
		int32_t             member_count = 0;
		Array<ThreadMember> added_members;
		Array<Snowflake>    removed_member_ids;

		ThreadMembersUpdateEvent(): Event(EventType::THREAD_MEMBERS_UPDATE) {}
		ThreadMembersUpdateEvent(Memory_Allocator allocator): 
			Event(EventType::THREAD_MEMBERS_UPDATE), added_members(allocator), removed_member_ids(allocator) {}
	};

	//
	//
	//

	using EventHandler = void(*)(struct Client *client, const struct Event *event);

	void DefOnEvent(struct Client *client, const struct Event *event) {
		TraceEx("Discord", StrFmt, StrArg(event->name));
	}

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

		EventHandler onevent = DefOnEvent;

		Memory_Arena *   scratch = nullptr;
		Memory_Allocator allocator;
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
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::Opcode::IDENTIFY);
		j.PushKey("d");
		Discord_Jsonify(client->identify, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendResume(Client *client) {
		Jsonify j(client->scratch);
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

		Jsonify j(client->scratch);
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
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::Opcode::REQUEST_GUILD_MEMBERS);
		j.PushKey("d");
		Discord_Jsonify(req_guild_mems, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendVoiceStateUpdate(Client *client, const VoiceStateUpdate &update_voice_state) {
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::Opcode::UPDATE_VOICE_STATE);
		j.PushKey("d");
		Discord_Jsonify(update_voice_state, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void SendPresenceUpdate(Client *client, const PresenceUpdate &presence_update) {
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Discord::Opcode::UPDATE_PRESECE);
		j.PushKey("d");
		Discord_Jsonify(presence_update, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}
}

static uint64_t Discord_ParseBigInt(String id) {
	uint64_t value = 0;
	for (auto ch : id) {
		value = value * 10 + (ch - '0');
	}
	return value;
}

static Discord::Snowflake Discord_ParseId(String id) {
	uint64_t value = Discord_ParseBigInt(id);
	return Discord::Snowflake(value);
}

static ptrdiff_t Discord_ParseTimestamp(String timestamp) {
	// 1990-12-31T23:59:60Z
	// 1996-12-19T16:39:57-08:00
	// 1937-01-01T12:00:27.87+00:20
	// 01234567890123456789

	char buffer[32];

	if (timestamp.length > sizeof(buffer) - 1)
		return 0;

	int len = (int)timestamp.length;
	memcpy(buffer, timestamp.data, len);
	memset(buffer + len, 0, sizeof(buffer) - len);

	buffer[4]  = 0;
	buffer[7]  = 0;
	buffer[10] = 0;
	buffer[13] = 0;
	buffer[16] = 0;

	long years  = strtol(buffer +  0, nullptr, 10);
	long months = strtol(buffer +  5, nullptr, 10);
	long days   = strtol(buffer +  8, nullptr, 10);
	long hours  = strtol(buffer + 11, nullptr, 10);
	long mins   = strtol(buffer + 14, nullptr, 10);

	char *end_ptr = nullptr;
	float frac    = strtof(buffer + 17, &end_ptr);
	long secs     = (long)frac;

	if (*end_ptr == '+' || *end_ptr == '-') {
		end_ptr[3] = 0;
		long offh  = strtol(end_ptr + 0, nullptr, 10);
		long offm  = strtol(end_ptr + 4, nullptr, 10);
		hours += offh;
		mins += offm;
	}

	constexpr long DaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	const long UNIX_YEAR = 1970;

	years -= UNIX_YEAR;

	months = Minimum(months, ArrayCount(DaysInMonth));
	months -= 1;
	for (long i = 0; i < months; ++i)
		days += DaysInMonth[i];
	days -= 1;

	ptrdiff_t epoch = 0;
	epoch += secs;
	epoch += mins * 60;
	epoch += hours * 60 * 60;
	epoch += days * 24 * 60 * 60;
	epoch += years * 365 * 24 * 60 * 60;

	return epoch;
}

static void Discord_Deserialize(const Json_Object &obj, Discord::User *user) {
	user->id            = Discord_ParseId(JsonGetString(obj, "id"));
	user->username      = JsonGetString(obj, "username");
	user->discriminator = JsonGetString(obj, "discriminator");
	user->avatar        = JsonGetString(obj, "avatar");
	user->bot           = JsonGetBool(obj, "bot");
	user->system        = JsonGetBool(obj, "system");
	user->mfa_enabled   = JsonGetBool(obj, "mfa_enabled");
	user->banner        = JsonGetString(obj, "banner");
	user->accent_color  = JsonGetInt(obj, "accent_color");
	user->locale        = JsonGetString(obj, "locale");
	user->verified      = JsonGetBool(obj, "verified");
	user->email         = JsonGetString(obj, "email");
	user->flags         = JsonGetInt(obj, "flags");
	user->premium_type  = (Discord::PremiumType)JsonGetInt(obj, "premium_type");
	user->public_flags  = JsonGetInt(obj, "public_flags");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ApplicationCommandPermission *perms) {
	perms->id         = Discord_ParseId(JsonGetString(obj, "id"));
	perms->type       = (Discord::ApplicationCommandPermissionType)JsonGetInt(obj, "type");
	perms->permission = JsonGetBool(obj, "permission");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ApplicationCommandPermissions *app_cmd_perms) {
	app_cmd_perms->id             = Discord_ParseId(JsonGetString(obj, "id"));
	app_cmd_perms->application_id = Discord_ParseId(JsonGetString(obj, "application_id"));
	app_cmd_perms->guild_id       = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Json_Array permissions = JsonGetArray(obj, "permissions");
	app_cmd_perms->permissions.Resize(permissions.count);

	for (ptrdiff_t index = 0; index < app_cmd_perms->permissions.count; ++index) {
		Discord_Deserialize(JsonGetObject(permissions[0]), &app_cmd_perms->permissions[index]);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Overwrite *overwrite) {
	overwrite->id    = Discord_ParseId(JsonGetString(obj, "id"));
	overwrite->type  = (Discord::OverwriteType)JsonGetInt(obj, "type");
	overwrite->allow = Discord_ParseBigInt(JsonGetString(obj, "allow"));
	overwrite->deny  = Discord_ParseBigInt(JsonGetString(obj, "deny"));
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ThreadMetadata *metadata) {
	metadata->archived              = JsonGetBool(obj, "archived");
	metadata->auto_archive_duration = JsonGetInt(obj, "auto_archive_duration");
	metadata->archive_timestamp     = Discord_ParseTimestamp(JsonGetString(obj, "archive_timestamp"));
	metadata->locked                = JsonGetBool(obj, "locked");
	metadata->invitable             = JsonGetBool(obj, "invitable");
	metadata->create_timestamp      = Discord_ParseTimestamp(JsonGetString(obj, "create_timestamp"));
}

static void Discord_Deserialize(const Json_Object &obj, Discord::GuildMember *member) {
	const Json *user = obj.Find("user");
	if (user) {
		member->user = new Discord::User;
		if (member->user) {
			Discord_Deserialize(JsonGetObject(*user), member->user);
		}
	}

	member->nick = JsonGetString(obj, "nick");
	member->avatar = JsonGetString(obj, "avatar");

	Json_Array roles = JsonGetArray(obj, "roles");
	member->roles.Resize(roles.count);
	for (ptrdiff_t index = 0; index < member->roles.count; ++index) {
		member->roles[index] = Discord_ParseId(JsonGetString(roles[index]));
	}

	member->joined_at                    = Discord_ParseTimestamp(JsonGetString(obj, "joined_at"));
	member->premium_since                = Discord_ParseTimestamp(JsonGetString(obj, "premium_since"));
	member->deaf                         = JsonGetBool(obj, "deaf");
	member->mute                         = JsonGetBool(obj, "mute");
	member->pending                      = JsonGetBool(obj, "pending");
	member->permissions                  = Discord_ParseBigInt(JsonGetString(obj, "permissions"));
	member->communication_disabled_until = JsonGetBool(obj, "communication_disabled_until");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ActivityTimestamps *timestamps) {
	timestamps->start = JsonGetInt(obj, "start");
	timestamps->end   = JsonGetInt(obj, "end");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ActivityEmoji *emoji) {
	emoji->name     = JsonGetString(obj, "name");
	emoji->id       = Discord_ParseId(JsonGetString(obj, "id"));
	emoji->animated = JsonGetBool(obj, "animated");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ActivityParty *party) {
	party->id = JsonGetString(obj, "id");
	Json_Array size = JsonGetArray(obj, "size");
	if (size.count == 2) {
		party->size[0] = JsonGetInt(size[0]);
		party->size[1] = JsonGetInt(size[1]);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ActivityAssets *party) {
	party->large_image = JsonGetString(obj, "large_image");
	party->large_text  = JsonGetString(obj, "large_text");
	party->small_image = JsonGetString(obj, "small_image");
	party->small_text  = JsonGetString(obj, "small_text");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ActivitySecrets *secrets) {
	secrets->join     = JsonGetString(obj, "join");
	secrets->spectate = JsonGetString(obj, "spectate");
	secrets->match    = JsonGetString(obj, "match");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ActivityButton *button) {
	button->label = JsonGetString(obj, "label");
	button->url   = JsonGetString(obj, "url");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Activity *activity) {
	activity->name       = JsonGetString(obj, "name");
	activity->type       = (Discord::ActivityType)JsonGetInt(obj, "type");
	activity->url        = JsonGetString(obj, "url");
	activity->created_at = JsonGetInt(obj, "created_at");

	Discord_Deserialize(JsonGetObject(obj, "timestamps"), &activity->timestamps);
	activity->application_id = Discord_ParseId(JsonGetString(obj, "application_id"));
	activity->details        = JsonGetString(obj, "details");
	activity->state          = JsonGetString(obj, "state");

	const Json *emoji = obj.Find("emoji");
	if (emoji) {
		if (emoji->type == JSON_TYPE_OBJECT) {
			activity->emoji = new Discord::ActivityEmoji;
			if (activity->emoji) {
				Discord_Deserialize(emoji->value.object, activity->emoji);
			}
		}
	}

	const Json *party = obj.Find("party");
	if (party) {
		activity->party = new Discord::ActivityParty;
		if (activity->party) {
			Discord_Deserialize(JsonGetObject(*party), activity->party);
		}
	}

	const Json *assets = obj.Find("assets");
	if (assets) {
		activity->assets = new Discord::ActivityAssets;
		if (activity->assets) {
			Discord_Deserialize(JsonGetObject(*assets), activity->assets);
		}
	}

	const Json *secrets = obj.Find("secrets");
	if (secrets) {
		activity->secrets = new Discord::ActivitySecrets;
		if (activity->secrets) {
			Discord_Deserialize(JsonGetObject(*secrets), activity->secrets);
		}
	}

	activity->instance = JsonGetBool(obj, "instance");
	activity->flags    = JsonGetInt(obj, "flags");

	Json_Array buttons = JsonGetArray(obj, "buttons");
	activity->buttons.Resize(buttons.count);
	for (ptrdiff_t index = 0; index < activity->buttons.count; ++index) {
		Discord_Deserialize(JsonGetObject(buttons[index]), &activity->buttons[index]);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ClientStatus *client_status) {
	client_status->desktop = JsonGetString(obj, "desktop");
	client_status->mobile  = JsonGetString(obj, "mobile");
	client_status->web     = JsonGetString(obj, "web");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Presence *presence) {
	Discord_Deserialize(JsonGetObject(obj, "user"), &presence->user);
	presence->guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	
	String status = JsonGetString(obj, "status");
	if (status == "idle")
		presence->status = Discord::StatusType::AFK;
	else if (status == "dnd")
		presence->status = Discord::StatusType::DO_NOT_DISTURB;
	else if (status == "online")
		presence->status = Discord::StatusType::ONLINE;
	else if (status == "offline")
		presence->status = Discord::StatusType::OFFLINE;
	else
		Unreachable();

	Json_Array activities = JsonGetArray(obj, "activities");
	presence->activities.Resize(activities.count);
	for (ptrdiff_t index = 0; index < presence->activities.count; ++index) {
		Discord_Deserialize(JsonGetObject(activities[index]), &presence->activities[index]);
	}

	Discord_Deserialize(JsonGetObject(obj, "client_status"), &presence->client_status);
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ThreadMember *member) {
	member->id             = Discord_ParseId(JsonGetString(obj, "id"));
	member->user_id        = Discord_ParseId(JsonGetString(obj, "user_id"));
	member->join_timestamp = Discord_ParseTimestamp(JsonGetString(obj, "join_timestamp"));
	member->flags          = JsonGetInt(obj, "flags");

	const Json *guild_member = obj.Find("member");
	if (guild_member) {
		member->member = new Discord::GuildMember;
		if (member->member) {
			Discord_Deserialize(JsonGetObject(*guild_member), member->member);
		}
	}

	const Json *presence = obj.Find("presence");
	if (presence) {
		member->presence = new Discord::Presence;
		if (member->presence) {
			Discord_Deserialize(JsonGetObject(*presence), member->presence);
		}
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Channel *channel) {
	channel->id       = Discord_ParseId(JsonGetString(obj, "id"));
	channel->type     = (Discord::ChannelType)JsonGetInt(obj, "type");
	channel->guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	channel->position = JsonGetInt(obj, "position", -1);

	Json_Array permission_overwrites = JsonGetArray(obj, "permission_overwrites");
	channel->permission_overwrites.Resize(permission_overwrites.count);
	for (ptrdiff_t index = 0; index < channel->permission_overwrites.count; ++index) {
		Discord_Deserialize(JsonGetObject(permission_overwrites[index]), &channel->permission_overwrites[index]);
	}

	channel->name                = JsonGetString(obj, "name");
	channel->topic               = JsonGetString(obj, "topic");
	channel->nsfw                = JsonGetBool(obj, "nsfw");
	channel->last_message_id     = Discord_ParseId(JsonGetString(obj, "last_message_id"));
	channel->bitrate             = JsonGetInt(obj, "bitrate");
	channel->user_limit          = JsonGetInt(obj, "user_limit");
	channel->rate_limit_per_user = JsonGetInt(obj, "rate_limit_per_user");

	Json_Array recipients = JsonGetArray(obj, "recipients");
	channel->recipients.Resize(recipients.count);
	for (ptrdiff_t index = 0; index < channel->recipients.count; ++index) {
		Discord_Deserialize(JsonGetObject(recipients[index]), &channel->recipients[index]);
	}

	channel->icon                          = JsonGetString(obj, "icon");
	channel->owner_id                      = Discord_ParseId(JsonGetString(obj, "owner_id"));
	channel->application_id                = Discord_ParseId(JsonGetString(obj, "application_id"));
	channel->parent_id                     = Discord_ParseId(JsonGetString(obj, "parent_id"));
	channel->last_pin_timestamp            = Discord_ParseTimestamp(JsonGetString(obj, "last_pin_timestamp"));
	channel->rtc_region                    = JsonGetString(obj, "rtc_region");
	channel->video_quality_mode            = JsonGetInt(obj, "video_quality_mode", 1);
	channel->message_count                 = JsonGetInt(obj, "message_count");
	channel->member_count                  = JsonGetInt(obj, "member_count");
	channel->default_auto_archive_duration = JsonGetInt(obj, "default_auto_archive_duration");
	channel->permissions                   = Discord_ParseBigInt(JsonGetString(obj, "permissions"));
	channel->flags                         = JsonGetInt(obj, "flags");

	const Json *thread_metadata = obj.Find("thread_metadata");
	if (thread_metadata) {
		channel->thread_metadata = new Discord::ThreadMetadata;
		if (channel->thread_metadata)
			Discord_Deserialize(JsonGetObject(*thread_metadata), channel->thread_metadata);
	}

	const Json *member = obj.Find("member");
	if (member) {
		channel->member = new Discord::ThreadMember;
		if (channel->member)
			Discord_Deserialize(JsonGetObject(*member), channel->member);
	}
}

//
//
//

typedef void(*Discord_Event_Handler)(Discord::Client *client, const Json &data);

static void Discord_EventHandlerNone(Discord::Client *client, const Json &data) {}

static void Discord_EventHandlerHello(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	client->heartbeat.interval = JsonGetFloat(obj, "heartbeat_interval", 45000);
	Discord::HelloEvent hello;
	hello.heartbeat_interval = (int32_t)client->heartbeat.interval;
	client->onevent(client, &hello);
}

static void Discord_EventHandlerReady(Discord::Client *client, const Json &data) {
	Discord::ReadyEvent ready;

	Json_Object obj = JsonGetObject(data);
	ready.v = JsonGetInt(obj, "v");
	Discord_Deserialize(JsonGetObject(obj, "user"), &ready.user);

	Json_Array guilds = JsonGetArray(obj, "guilds");
	ready.guilds.Resize(guilds.count);

	for (ptrdiff_t index = 0; index < ready.guilds.count; ++index) {
		Json_Object unavailable_guild = JsonGetObject(guilds[index]);
		ready.guilds[index] = Discord_ParseId(JsonGetString(unavailable_guild, "id"));
	}

	ready.session_id = JsonGetString(obj, "session_id");

	Json_Array shard = JsonGetArray(obj, "shard");
	ready.shard[0] = JsonGetInt(shard[0], 0);
	ready.shard[1] = JsonGetInt(shard[1], 1);

	Json_Object application = JsonGetObject(obj, "application");
	ready.application.id = Discord_ParseId(JsonGetString(application, "id"));
	ready.application.flags = JsonGetInt(application, "flags");

	if (ready.session_id.length) {
		if (client->session_id.length)
			MemoryFree(client->session_id.data, client->session_id.length + 1, client->allocator);
		client->session_id = StrDup(ready.session_id, client->allocator);
	}

	client->onevent(client, &ready);
}

static void Discord_EventHandlerResumed(Discord::Client *client, const Json &data) {
	Discord::ResumedEvent resumed;
	client->onevent(client, &resumed);
}

static void Discord_EventHandlerReconnect(Discord::Client *client, const Json &data) {
	Discord::ReconnectEvent reconnect;
	client->onevent(client, &reconnect);
	Unimplemented();
}

static void Discord_EventHandlerInvalidSession(Discord::Client *client, const Json &data) {
	Discord::InvalidSessionEvent invalid_session;
	invalid_session.resumable = JsonGetBool(data);
	client->onevent(client, &invalid_session);
	Unimplemented();
}

static void Discord_EventHandlerApplicationCommandPermissionsUpdate(Discord::Client *client, const Json &data) {
	Discord::ApplicationCommandPermissionsUpdateEvent app_cmd_perms_update;
	Discord_Deserialize(JsonGetObject(data), &app_cmd_perms_update.permissions);
	client->onevent(client, &app_cmd_perms_update);
}

static void Discord_EventHandlerChannelCreate(Discord::Client *client, const Json &data) {
	Discord::ChannelCreateEvent channel;
	Discord_Deserialize(JsonGetObject(data), &channel.channel);
	client->onevent(client, &channel);
}

static void Discord_EventHandlerChannelUpdate(Discord::Client *client, const Json &data) {
	Discord::ChannelCreateEvent channel;
	Discord_Deserialize(JsonGetObject(data), &channel.channel);
	client->onevent(client, &channel);
}

static void Discord_EventHandlerChannelDelete(Discord::Client *client, const Json &data) {
	Discord::ChannelCreateEvent channel;
	Discord_Deserialize(JsonGetObject(data), &channel.channel);
	client->onevent(client, &channel);
}

static void Discord_EventHandlerChannelPinsUpdate(Discord::Client *client, const Json &data) {
	Discord::ChannelPinsUpdateEvent pins;
	Json_Object obj         = JsonGetObject(data);
	pins.guild_id           = Discord_ParseId(JsonGetString(obj, "guild_id"));
	pins.channel_id         = Discord_ParseId(JsonGetString(obj, "channel_id"));
	pins.last_pin_timestamp = Discord_ParseTimestamp(JsonGetString(obj, "last_pin_timestamp"));
	client->onevent(client, &pins);
}

static void Discord_EventHandlerThreadCreate(Discord::Client *client, const Json &data) {
	Discord::ThreadCreateEvent thread;
	Json_Object obj = JsonGetObject(data);
	Discord_Deserialize(obj, &thread.channel);
	thread.newly_created = JsonGetBool(obj, "newly_created");
	client->onevent(client, &thread);
}

static void Discord_EventHandlerThreadUpdate(Discord::Client *client, const Json &data) {
	Discord::ThreadUpdateEvent thread;
	Json_Object obj = JsonGetObject(data);
	Discord_Deserialize(obj, &thread.channel);
	client->onevent(client, &thread);
}

static void Discord_EventHandlerThreadDelete(Discord::Client *client, const Json &data) {
	Discord::ThreadDeleteEvent thread;
	Json_Object obj  = JsonGetObject(data);
	thread.id        = Discord_ParseId(JsonGetString(obj, "id"));
	thread.guild_id  = Discord_ParseId(JsonGetString(obj, "guild_id"));
	thread.parent_id = Discord_ParseId(JsonGetString(obj, "parent_id"));
	thread.type      = (Discord::ChannelType)JsonGetInt(obj, "type");
	client->onevent(client, &thread);
}

static void Discord_EventHandlerThreadListSync(Discord::Client *client, const Json &data) {
	Discord::ThreadListSyncEvent sync;
	Json_Object obj = JsonGetObject(data);
	sync.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Json_Array channel_ids = JsonGetArray(obj, "channel_ids");
	sync.channel_ids.Resize(channel_ids.count);
	for (ptrdiff_t index = 0; index < sync.channel_ids.count; ++index) {
		sync.channel_ids[index] = Discord_ParseId(JsonGetString(channel_ids[index]));
	}

	Json_Array threads = JsonGetArray(obj, "threads");
	sync.threads.Resize(threads.count);
	for (ptrdiff_t index = 0; index < sync.threads.count; ++index) {
		Discord_Deserialize(JsonGetObject(threads[index]), &sync.threads[index]);
	}

	Json_Array members = JsonGetArray(obj, "members");
	sync.members.Resize(members.count);
	for (ptrdiff_t index = 0; index < sync.members.count; ++index) {
		Discord_Deserialize(JsonGetObject(members[index]), &sync.threads[index]);
	}

	client->onevent(client, &sync);
}

static void Discord_EventHandlerThreadMemberUpdate(Discord::Client *client, const Json &data) {
	Discord::ThreadMemberUpdateEvent mem_update;
	Json_Object obj = JsonGetObject(data);
	Discord_Deserialize(obj, &mem_update.member);
	mem_update.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent(client, &mem_update);
}

static void Discord_EventHandlerThreadMembersUpdate(Discord::Client *client, const Json &data) {
	Discord::ThreadMembersUpdateEvent mems_update;
	Json_Object obj          = JsonGetObject(data);
	mems_update.id           = Discord_ParseId(JsonGetString(obj, "id"));
	mems_update.id           = Discord_ParseId(JsonGetString(obj, "guild_id"));
	mems_update.member_count = JsonGetInt(obj, "member_count");

	Json_Array added_members = JsonGetArray(obj, "added_members");
	mems_update.added_members.Resize(added_members.count);
	for (ptrdiff_t index = 0; index < mems_update.added_members.count; ++index) {
		Discord_Deserialize(JsonGetObject(added_members[index]), &mems_update.added_members[index]);
	}

	Json_Array removed_member_ids = JsonGetArray(obj, "removed_member_ids");
	mems_update.removed_member_ids.Resize(removed_member_ids.count);
	for (ptrdiff_t index = 0; index < mems_update.removed_member_ids.count; ++index) {
		mems_update.removed_member_ids[index] = Discord_ParseId(JsonGetString(removed_member_ids[index]));
	}

	client->onevent(client, &mems_update);
}

static constexpr Discord_Event_Handler DiscordEventHandlers[] = {
	Discord_EventHandlerNone, Discord_EventHandlerHello, Discord_EventHandlerReady,
	Discord_EventHandlerResumed, Discord_EventHandlerReconnect, Discord_EventHandlerInvalidSession,
	Discord_EventHandlerApplicationCommandPermissionsUpdate, Discord_EventHandlerChannelCreate,
	Discord_EventHandlerChannelUpdate, Discord_EventHandlerChannelDelete, Discord_EventHandlerChannelPinsUpdate,
	Discord_EventHandlerThreadCreate, Discord_EventHandlerThreadUpdate, Discord_EventHandlerThreadDelete,
	Discord_EventHandlerThreadListSync, Discord_EventHandlerThreadMemberUpdate, Discord_EventHandlerThreadMembersUpdate,

	Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
	Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone, Discord_EventHandlerNone,
};
static_assert(ArrayCount(DiscordEventHandlers) == ArrayCount(Discord::EventNames), "");

static void Discord_HandleEvent(Discord::Client *client, String event, const Json &data) {
	for (int index = 0; index < ArrayCount(Discord::EventNames); ++index) {
		if (event == Discord::EventNames[index]) {
			TraceEx("Discord", "Event " StrFmt, StrArg(event));
			DiscordEventHandlers[index](client, data);
			return;
		}
	}
	LogErrorEx("Discord", "Unknown event: " StrFmt, event);
}

static void Discord_HandleWebsocketEvent(Discord::Client *client, const Websocket_Event &event) {
	if (event.type != WEBSOCKET_EVENT_TEXT)
		return;

	Json json;
	if (JsonParse(event.message, &json)) {
		Json_Object payload = JsonGetObject(json);
		int         opcode  = JsonGetInt(payload, "op");
		Json        data    = JsonGet(payload, "d");

		if (opcode == (int)Discord::Opcode::DISPATH) {
			client->sequence  = JsonGetInt(payload, "s", client->sequence);
			String event_name = JsonGetString(payload, "t");
			Discord_HandleEvent(client, event_name, data);
			return;
		}

		if (opcode == (int)Discord::Opcode::HEARTBEAT) {
			TraceEx("Discord", "Heartbeat (%d)", client->heartbeat.count);
			Discord::SendHearbeat(client);
			return;
		}

		if (opcode == (int)Discord::Opcode::RECONNECT) {
			Discord_EventHandlerReconnect(client, data);
			return;
		}

		if (opcode == (int)Discord::Opcode::INVALID_SESSION) {
			Discord_EventHandlerInvalidSession(client, data);
			return;
		}

		if (opcode == (int)Discord::Opcode::HELLO) {
			Discord_EventHandlerHello(client, data);
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

void TestEventHandler(Discord::Client *client, const Discord::Event *event) {
	if (event->type == Discord::EventType::READY) {
		auto ready = (Discord::ReadyEvent *)event;
		Trace("Bot online " StrFmt "#" StrFmt, StrArg(ready->user.username), StrArg(ready->user.discriminator));
		Trace("Discord Gateway Version : %d", ready->v);
		return;
	}

	if (event->type == Discord::EventType::APPLICATION_COMMAND_PERMISSIONS_UPDATE) {
		auto perms = (Discord::ApplicationCommandPermissionsUpdateEvent *)event;
		Trace("Permission updates: " StrFmt, StrArg(perms->name));
		return;
	}

	if (event->type == Discord::EventType::CHANNEL_CREATE) {
		auto channel = (Discord::ChannelCreateEvent *)event;
		Trace("Channel created: " StrFmt, StrArg(channel->channel.name));
		return;
	}
	
	if (event->type == Discord::EventType::CHANNEL_UPDATE) {
		auto channel = (Discord::ChannelUpdateEvent *)event;
		Trace("Channel updated: " StrFmt, StrArg(channel->channel.name));
		return;
	}
	
	if (event->type == Discord::EventType::CHANNEL_DELETE) {
		auto channel = (Discord::ChannelDeleteEvent *)event;
		Trace("Channel deleted: " StrFmt, StrArg(channel->channel.name));
		return;
	}
	
	if (event->type == Discord::EventType::CHANNEL_PINS_UPDATE) {
		auto pins = (Discord::ChannelPinsUpdateEvent *)event;
		Trace("Pins Updated");
		return;
	}

	if (event->type == Discord::EventType::THREAD_CREATE) {
		auto thread = (Discord::ThreadCreateEvent *)event;
		Trace("Thread created: " StrFmt, StrArg(thread->channel.name));
		return;
	}

	if (event->type == Discord::EventType::THREAD_UPDATE) {
		auto thread = (Discord::ThreadUpdateEvent *)event;
		Trace("Thread updated: " StrFmt, StrArg(thread->channel.name));
		return;
	}

	if (event->type == Discord::EventType::THREAD_DELETE) {
		auto thread = (Discord::ThreadDeleteEvent *)event;
		Trace("Thread of id %zu deleted", thread->id.value);
		return;
	}

	if (event->type == Discord::EventType::THREAD_LIST_SYNC) {
		auto thread = (Discord::ThreadListSyncEvent *)event;
		Trace("Thread List Sync");
		return;
	}

	if (event->type == Discord::EventType::THREAD_MEMBER_UPDATE) {
		auto thread = (Discord::ThreadMemberUpdateEvent *)event;
		Trace("Thread member update");
		return;
	}

	if (event->type == Discord::EventType::THREAD_MEMBERS_UPDATE) {
		auto thread = (Discord::ThreadMembersUpdateEvent *)event;
		if (thread->added_members.count) {
			Trace("Thread Member added: " StrFmt, StrArg(thread->added_members[0].member->nick));
		} else {
			Trace("Thread Members updated: %d", thread->member_count);
		}
		return;
	}
}

int main(int argc, char **argv) {
	InitThreadContext(0);
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	Discord::Client client;
	client.scratch   = MemoryArenaAllocate(MegaBytes(64));
	client.allocator = ThreadContextDefaultParams.allocator;

	if (!client.scratch) {
		return 1; // @todo log error
	}

	client.token         = StrDup(String(argv[1], strlen(argv[1])), client.allocator);
	String authorization = FmtStr(client.scratch, "Bot " StrFmt, StrArg(client.token));

	Http *http = Http_Connect("https://discord.com");
	if (!http) {
		return 1;
	}

	Http_Request req;
	Http_InitRequest(&req);
	Http_SetHost(&req, http);
	Http_SetHeader(&req, HTTP_HEADER_AUTHORIZATION, authorization);
	Http_SetHeader(&req, HTTP_HEADER_USER_AGENT, Discord::UserAgent);

	Http_Response res;
	if (!Http_Get(http, "/api/v10/gateway/bot", req, &res, client.scratch)) {
		return 1; // @todo EndTemporaryMemory? MemoryArenaFree? Http_Disconnect?
	}

	Json json;
	if (!JsonParse(res.body, &json, MemoryArenaAllocator(client.scratch))) {
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
	url = StrContat(url, "/?v=9&encoding=json", client.scratch);

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

	MemoryArenaReset(client.scratch);

	int intents = 0;
	intents |= Discord::Intent::GUILDS;
	intents |= Discord::Intent::GUILD_MEMBERS;
	intents |= Discord::Intent::GUILD_MESSAGES;
	intents |= Discord::Intent::GUILD_MESSAGE_REACTIONS;
	intents |= Discord::Intent::DIRECT_MESSAGES;
	intents |= Discord::Intent::DIRECT_MESSAGE_REACTIONS;
	intents |= Discord::Intent::GUILD_INTEGRATIONS;

	Discord::PresenceUpdate presence(client.allocator);
	presence.status = Discord::StatusType::DO_NOT_DISTURB;
	auto activity   = presence.activities.Add();
	activity->name  = "Twitch";
	activity->url   = "https://www.twitch.tv/ashishzero";
	activity->type  = Discord::ActivityType::STREAMING;

	client.websocket = websocket;
	client.identify  = Discord::Identify(client.token, intents, &presence);
	client.onevent   = TestEventHandler;

	clock_t counter = clock();
	client.heartbeat.remaining = client.heartbeat.interval;

	ThreadContext.allocator = MemoryArenaAllocator(client.scratch);

	while (Websocket_IsConnected(websocket)) {
		Websocket_Event event;
		Websocket_Result res = Websocket_Receive(websocket, &event, client.scratch, (int)client.heartbeat.remaining);

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

		MemoryArenaReset(client.scratch);
	}

	Websocket_Close(websocket, WEBSOCKET_CLOSE_GOING_AWAY);

	Net_Shutdown();

	return 0;
}
