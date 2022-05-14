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
		operator bool() { return value != 0; }
	};

	static inline bool operator==(Snowflake a, Snowflake b) { return a.value == b.value; }
	static inline bool operator!=(Snowflake a, Snowflake b) { return a.value != b.value; }

	struct Timestamp {
		ptrdiff_t value = 0;
		Timestamp() = default;
		Timestamp(ptrdiff_t val): value(val) {}
	};

	static inline bool operator==(Timestamp a, Timestamp b) { return a.value == b.value; }
	static inline bool operator!=(Timestamp a, Timestamp b) { return a.value != b.value; }

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


	enum class InviteTargetType {
		NONE = 0, STREAM = 1, EMBEDDED_APPLICATION = 2
	};

	//
	//
	//

	enum class ApplicationCommandPermissionType {
		NONE = 0, ROLE = 1, USER = 2, CHANNEL = 3
	};

	struct ApplicationCommandPermission {
		Snowflake                        id;
		ApplicationCommandPermissionType type = ApplicationCommandPermissionType::NONE;
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

	enum class MembershipState {
		NONE = 0, INVITED = 1, ACCEPTED = 2
	};

	struct TeamMember {
		MembershipState membership_state = MembershipState::NONE;
		Array<String>   permissions;
		Snowflake       team_id;
		User            user;

		TeamMember() = default;
		TeamMember(Memory_Allocator allocator): permissions(allocator) {}
	};

	struct Team {
		String            icon;
		Snowflake         id;
		Array<TeamMember> members;
		String            name;
		Snowflake         owner_user_id;

		Team() = default;
		Team(Memory_Allocator allocator): members(allocator) {}
	};

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

	enum class ApplicationCommandType {
		NONE = 0, CHAT_INPUT = 1, USER = 2, MESSAGE = 3
	};

	enum class ApplicationCommandOptionType {
		NONE              = 0,
		SUB_COMMAND       = 1,
		SUB_COMMAND_GROUP = 2,
		STRING            = 3,
		INTEGER           = 4,
		BOOLEAN           = 5,
		USER              = 6,
		CHANNEL           = 7,
		ROLE              = 8,
		MENTIONABLE       = 9,
		NUMBER            = 10,
		ATTACHMENT        = 11
	};

	struct ApplicationCommandInteractionDataOption {
		union Value {
			String  string;
			int32_t integer;
			float   number;
			Value() {}
		};

		String                                         name;
		ApplicationCommandOptionType                   type = ApplicationCommandOptionType::NONE;
		Value                                          value;
		Array<ApplicationCommandInteractionDataOption> options;
		bool                                           focused = false;

		ApplicationCommandInteractionDataOption() = default;
		ApplicationCommandInteractionDataOption(Memory_Allocator allocator):
			options(allocator) {}
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
		Team *          team = nullptr;
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
		Timestamp start = 0;
		Timestamp end   = 0;
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
		Timestamp             created_at = 0;
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
		Timestamp        joined_at;
		Timestamp        premium_since;
		bool             deaf = false;
		bool             mute = false;
		bool             pending = false;
		Permission       permissions = 0;
		Timestamp        communication_disabled_until = 0;

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
		Timestamp archive_timestamp;
		bool      locked = false;
		bool      invitable = false;
		Timestamp create_timestamp = 0;
	};

	struct ThreadMember {
		Snowflake      id;
		Snowflake      user_id;
		Timestamp      join_timestamp = 0;
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
		Timestamp        last_pin_timestamp = 0;
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

	enum class VerificationLevel {
		NONE = 0, LOW = 1, MEDIUM = 2, HIGH = 3, VERY_HIGH = 4
	};

	enum class MessageNotificationLevel {
		ALL_MESSAGES = 0, ONLY_MENTIONS = 1
	};

	enum class ExplicitContentFilterLevel {
		DISABLED = 0, MEMBERS_WITHOUT_ROLES = 1, ALL_MEMBERS = 2
	};

	enum class MFALevel {
		NONE = 0, ELEVATED = 1
	};

	typedef int32_t SystemChannelFlag;
	struct SystemChannelFlagBit {
		enum {
			SUPPRESS_JOIN_NOTIFICATIONS           = 1 << 0,
			SUPPRESS_PREMIUM_SUBSCRIPTIONS        = 1 << 1,
			SUPPRESS_GUILD_REMINDER_NOTIFICATIONS = 1 << 2,
			SUPPRESS_JOIN_NOTIFICATION_REPLIES    = 1 << 3
		};
	};

	enum class PremiumTier {
		NONE = 0, TIER_1 = 1, TIER_2 = 2, TIER_3 = 3
	};

	enum class GuildNSFWLevel {
		DEFAULT = 0, EXPLICIT = 1, SAFE = 2, AGE_RESTRICTED = 3
	};

	struct RoleTag {
		Snowflake bot_id;
		Snowflake integration_id;
		bool      premium_subscriber = false;
	};

	struct Role {
		Snowflake  id;
		String     name;
		int32_t    color = 0;
		bool       hoist = false;
		String     icon;
		String     unicode_emoji;
		int32_t    position = 0;
		Permission permissions = 0;
		bool       managed;
		bool       mentionable;
		RoleTag *  tags = nullptr;
	};

	struct Emoji {
		Snowflake   id;
		String      name;
		Array<Role> roles;
		User *      user = nullptr;
		bool        require_colons = false;
		bool        managed = false;
		bool        animated = false;
		bool        available = false;

		Emoji() = default;
		Emoji(Memory_Allocator allocator): roles(allocator) {}
	};

	enum class GuildFeature {
		ANIMATED_BANNER,
		ANIMATED_ICON,
		BANNER,
		COMMERCE,
		COMMUNITY,
		DISCOVERABLE,
		FEATURABLE,
		INVITE_SPLASH,
		MEMBER_VERIFICATION_GATE_ENABLED,
		MONETIZATION_ENABLED,
		MORE_STICKERS,
		NEWS,
		PARTNERED,
		PREVIEW_ENABLED,
		PRIVATE_THREADS,
		ROLE_ICONS,
		TICKETED_EVENTS_ENABLED,
		VANITY_URL,
		VERIFIED,
		VIP_REGIONS,
		WELCOME_SCREEN_ENABLED,

		GUILD_FEATURE_COUNT
	};

	struct WelcomeScreenChannel {
		Snowflake channel_id;
		String    description;
		Snowflake emoji_id;
		String    emoji_name;
	};

	struct WelcomeScreen {
		String                      description;
		Array<WelcomeScreenChannel> welcome_channels;

		WelcomeScreen() = default;
		WelcomeScreen(Memory_Allocator allocator): welcome_channels(allocator) {}
	};

	enum class StickerType {
		NONE = 0, STANDARD = 1, GUILD = 2
	};

	enum class StickerFormatType {
		NONE = 0, PNG = 1, APNG = 2, LOTTIE = 3
	};

	struct Sticker {
		Snowflake         id;
		Snowflake         pack_id;
		String            name;
		String            description;
		String            tags;
		StickerType       type = StickerType::NONE;
		StickerFormatType format_type = StickerFormatType::NONE;
		bool              available = false;
		Snowflake         guild_id;
		User *            user = nullptr;
		int32_t           sort_value = 0;
	};

	struct Guild {
		Snowflake                  id;
		String                     name;
		String                     icon;
		String                     icon_hash;
		String                     splash;
		String                     discovery_splash;
		bool                       owner = false;
		Snowflake                  owner_id;
		Permission                 permissions;
		Snowflake                  afk_channel_id;
		int32_t                    afk_timeout = 0;
		bool                       widget_enabled = false;
		Snowflake                  widget_channel_id;
		VerificationLevel          verification_level = VerificationLevel::NONE;
		MessageNotificationLevel   default_message_notifications = MessageNotificationLevel::ALL_MESSAGES;
		ExplicitContentFilterLevel explicit_content_filter = ExplicitContentFilterLevel::DISABLED;
		Array<Role>                roles;
		Array<Emoji>               emojis;
		Array<GuildFeature>        features;
		MFALevel                   mfa_level = MFALevel::NONE;
		Snowflake                  application_id;
		Snowflake                  system_channel_id;
		SystemChannelFlag          system_channel_flags = 0;
		Snowflake                  rules_channel_id;
		int32_t                    max_presences = 0;
		int32_t                    max_members = 0;
		String                     vanity_url_code;
		String                     description;
		String                     banner;
		PremiumTier                premium_tier = PremiumTier::NONE;
		int32_t                    premium_subscription_count = 0;
		String                     preferred_locale;
		Snowflake                  public_updates_channel_id;
		int32_t                    max_video_channel_users = 0;
		int32_t                    approximate_member_count = 0;
		int32_t                    approximate_presence_count = 0;
		WelcomeScreen *            welcome_screen = nullptr;
		GuildNSFWLevel             nsfw_level = GuildNSFWLevel::DEFAULT;
		Array<Sticker>             stickers;
		bool                       premium_progress_bar_enabled = false;

		Guild() = default;
		Guild(Memory_Allocator allocator):
			roles(allocator), emojis(allocator), features(allocator), stickers(allocator) {}
	};

	struct UnavailableGuild {
		Snowflake id;
		bool unavailable = false;
	};

	enum class PrivacyLevel {
		NONE = 0, PUBLIC = 1, GUILD_ONLY = 2
	};

	struct StageInstance {
		Snowflake    id;
		Snowflake    guild_id;
		Snowflake    channel_id;
		String       topic;
		PrivacyLevel privacy_level = PrivacyLevel::NONE;
		bool         discoverable_disabled = false;
		Snowflake    guild_scheduled_event_id;
	};

	struct VoiceState {
		Snowflake    guild_id;
		Snowflake    channel_id;
		Snowflake    user_id;
		GuildMember *member = nullptr;
		String       session_id;
		bool         deaf = false;
		bool         mute = false;
		bool         self_deaf = false;
		bool         self_mute = false;
		bool         self_stream = false;
		bool         self_video = false;
		bool         suppress = false;
		Timestamp    request_to_speak_timestamp = 0;
	};

	enum class GuildScheduledEventPrivacyLevel {
		NONE = 0, GUILD_ONLY = 2
	};

	enum class GuildScheduledEventEntityType {
		NONE = 0, STAGE_INSTANCE = 1, VOICE = 2, EXTERNAL = 3
	};

	enum class GuildScheduledEventStatus {
		NONE = 0, SCHEDULED = 1, ACTIVE = 2, COMPLETED = 3, CANCELED = 4
	};

	struct GuildScheduledEventEntityMetadata {
		String location;
	};

	struct GuildScheduledEvent {
		Snowflake                          id;
		Snowflake                          guild_id;
		Snowflake                          channel_id;
		Snowflake                          creator_id;
		String                             name;
		String                             description;
		Timestamp                          scheduled_start_time = 0;
		Timestamp                          scheduled_end_time = 0;
		GuildScheduledEventPrivacyLevel    privacy_level = GuildScheduledEventPrivacyLevel::NONE;
		GuildScheduledEventStatus          status = GuildScheduledEventStatus::NONE;
		GuildScheduledEventEntityType      entity_type = GuildScheduledEventEntityType::NONE;
		Snowflake                          entity_id;
		GuildScheduledEventEntityMetadata *entity_metadata = nullptr;
		User *                             creator = nullptr;
		int32_t                            user_count = 0;
		String                             image;
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
			MESSAGE_CONTENT           = 1 << 15,
			GUILD_SCHEDULED_EVENTS    = 1 << 16,
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

	enum class IntegrationExpireBehavior {
		REMOVE_ROLE = 0, KICK = 1
	};

	struct IntegrationAccount {
		String id;
		String name;
	};

	struct IntegrationApplication {
		Snowflake id;
		String    name;
		String    icon;
		String    description;
		User *    bot = nullptr;
	};

	struct Integration {
		Snowflake                 id;
		String                    name;
		String                    type;
		bool                      enabled = false;
		bool                      syncing = false;
		Snowflake                 role_id;
		bool                      enable_emoticons;
		IntegrationExpireBehavior expire_behavior = IntegrationExpireBehavior::REMOVE_ROLE;
		int32_t                   expire_grace_period = 0;
		User *                    user = nullptr;
		IntegrationAccount        account;
		Timestamp                 synced_at = 0;
		int32_t                   subscriber_count = 0;
		bool                      revoked = false;
		IntegrationApplication *  application = nullptr;
	};

	//
	//
	//

	struct SelectOption {
		String  label;
		String  value;
		String  description;
		Emoji * emoji = nullptr;
		bool    isdefault = false;
	};

	enum class ComponentType {
		NONE = 0, ACTION_ROW = 1, BUTTON = 2, SELECT_MENU = 3, TEXT_INPUT = 4
	};

	enum class ButtonStyle {
		NONE = 0, PRIMARY = 1, SECONDARY = 2, SUCCESS = 3, DANGER = 4, LINK = 5
	};

	enum class TextInputStyle {
		NONE = 0, SHORT = 1, PARAGRAPH = 2
	};

	struct Component {
		struct ActionRow {
			Array<Component *> components;

			ActionRow() = default;
			ActionRow(Memory_Allocator allocator): components(allocator) {}
		};

		struct Button {
			ButtonStyle style = ButtonStyle::NONE;
			String      label;
			Emoji *     emoji = nullptr;
			String      custom_id;
			String      url;
			bool        disabled = false;
		};

		struct SelectMenu {
			String              custom_id;
			Array<SelectOption> options;
			String              placeholder;
			int32_t             min_values = 0;
			int32_t             max_values = 0;
			bool                disabled = false;

			SelectMenu() = default;
			SelectMenu(Memory_Allocator allocator): options(allocator) {}
		};

		struct TextInput {
			String         custom_id;
			TextInputStyle style = TextInputStyle::NONE;
			String         label;
			int32_t        min_length = 0;
			int32_t        max_length = 0;
			bool           required = false;
			String         value;
			String         placeholder;
		};

		union Data {
			ActionRow  action_row;
			Button     button;
			SelectMenu select_menu;
			TextInput  text_input;
			Data(){}
		};

		ComponentType type = ComponentType::NONE;
		Data          data;

		Component() { memset(&data, 0, sizeof(data)); }
	};

	//
	//
	//

	struct EmbedFooter {
		String text;
		String icon_url;
		String proxy_icon_url;
	};

	struct EmbedImage {
		String  url;
		String  proxy_url;
		int32_t height = 0;
		int32_t width = 0;
	};

	struct EmbedThumbnail {
		String  url;
		String  proxy_url;
		int32_t height = 0;
		int32_t width = 0;
	};

	struct EmbedVideo {
		String  url;
		String  proxy_url;
		int32_t height = 0;
		int32_t width = 0;
	};

	struct EmbedProvider {
		String name;
		String url;
	};

	struct EmbedAuthor {
		String name;
		String url;
		String icon_url;
		String proxy_icon_url;
	};

	struct EmbedField {
		String name;
		String value;
		bool   isinline = false;
	};

	struct Embed {
		String            title;
		String            type;
		String            description;
		String            url;
		Timestamp         timestamp = 0;
		int32_t           color = 0;
		EmbedFooter *     footer = nullptr;
		EmbedImage *      image = nullptr;
		EmbedThumbnail *  thumbnail = nullptr;
		EmbedVideo *      video = nullptr;
		EmbedProvider *   provider = nullptr;
		EmbedAuthor *     author = nullptr;
		Array<EmbedField> fields;

		Embed() = default;
		Embed(Memory_Allocator allocator): fields(allocator) {}
	};

	struct Attachment {
		Snowflake id;
		String    filename;
		String    description;
		String    content_type;
		int32_t   size = 0;
		String    url;
		String    proxy_url;
		int32_t   height = 0;
		int32_t   width = 0;
		bool      ephemeral = false;
	};

	struct Mentions {
		User        user;
		GuildMember member;
	};

	struct ChannelMention {
		Snowflake   id;
		Snowflake   guild_id;
		ChannelType type = ChannelType::GUILD_TEXT;
		String      name;
	};

	struct Reaction {
		int32_t count = 0;
		bool    me = false;
		Emoji   emoji;

		Reaction() = default;
		Reaction(Memory_Allocator allocator): emoji(allocator) {}
	};

	enum class MessageActivityType {
		NONE = 0, JOIN = 1, SPECTATE = 2, LISTEN = 3, JOIN_REQUEST = 5,
	};

	struct MessageActivity {
		MessageActivityType type = MessageActivityType::NONE;
		String              party_id;
	};

	enum class MessageType {
		DEFAULT                                      = 0,
		RECIPIENT_ADD                                = 1,
		RECIPIENT_REMOVE                             = 2,
		CALL                                         = 3,
		CHANNEL_NAME_CHANGE                          = 4,
		CHANNEL_ICON_CHANGE                          = 5,
		CHANNEL_PINNED_MESSAGE                       = 6,
		GUILD_MEMBER_JOIN                            = 7,
		USER_PREMIUM_GUILD_SUBSCRIPTION              = 8,
		USER_PREMIUM_GUILD_SUBSCRIPTION_TIER_1       = 9,
		USER_PREMIUM_GUILD_SUBSCRIPTION_TIER_2       = 10,
		USER_PREMIUM_GUILD_SUBSCRIPTION_TIER_3       = 11,
		CHANNEL_FOLLOW_ADD                           = 12,
		GUILD_DISCOVERY_DISQUALIFIED                 = 14,
		GUILD_DISCOVERY_REQUALIFIED                  = 15,
		GUILD_DISCOVERY_GRACE_PERIOD_INITIAL_WARNING = 16,
		GUILD_DISCOVERY_GRACE_PERIOD_FINAL_WARNING   = 17,
		THREAD_CREATED                               = 18,
		REPLY                                        = 19,
		CHAT_INPUT_COMMAND                           = 20,
		THREAD_STARTER_MESSAGE                       = 21,
		GUILD_INVITE_REMINDER                        = 22,
		CONTEXT_MENU_COMMAND                         = 23,
	};

	struct MessageReference {
		Snowflake message_id;
		Snowflake channel_id;
		Snowflake guild_id;
		bool      fail_if_not_exists = false;
	};

	typedef int32_t MessageFlag;
	struct MessageFlagBit {
		enum {
			CROSSPOSTED                            = 1 << 0,
			IS_CROSSPOST                           = 1 << 1,
			SUPPRESS_EMBEDS                        = 1 << 2,
			SOURCE_MESSAGE_DELETED                 = 1 << 3,
			URGENT                                 = 1 << 4,
			HAS_THREAD                             = 1 << 5,
			EPHEMERAL                              = 1 << 6,
			LOADING                                = 1 << 7,
			FAILED_TO_MENTION_SOME_ROLES_IN_THREAD = 1 << 8
		};
	};

	enum class InteractionType {
		NONE                             = 0,
		PING                             = 1,
		APPLICATION_COMMAND              = 2,
		MESSAGE_COMPONENT                = 3,
		APPLICATION_COMMAND_AUTOCOMPLETE = 4,
		MODAL_SUBMIT                     = 5
	};

	struct MessageInteraction {
		Snowflake       id;
		InteractionType type = InteractionType::NONE;
		String          name;
		User            user;
		GuildMember *   member = nullptr;
	};

	struct StickerItem {
		Snowflake         id;
		String            name;
		StickerFormatType format_type = StickerFormatType::NONE;
	};

	struct Message {
		Snowflake             id;
		Snowflake             channel_id;
		Snowflake             guild_id;
		User                  author;
		GuildMember *         member = nullptr;
		String                content;
		Timestamp             timestamp = 0;
		Timestamp             edited_timestamp = 0;
		bool                  tts = false;
		bool                  mention_everyone = false;
		Array<Mentions>       mentions;
		Array<Snowflake>      mention_roles;
		Array<ChannelMention> mention_channels;
		Array<Attachment>     attachments;
		Array<Embed>          embeds;
		Array<Reaction>       reactions;
		String                nonce;
		bool                  pinned = false;
		Snowflake             webhook_id;
		MessageType           type = MessageType::DEFAULT;
		MessageActivity *     activity = nullptr;
		Application *         application = nullptr;
		Snowflake             application_id;
		MessageReference *    message_reference = nullptr;
		MessageFlag           flags = 0;
		Message *             referenced_message = nullptr;
		MessageInteraction *  interaction = nullptr;
		Channel *             thread = nullptr;
		Array<Component>      components;
		Array<StickerItem>    sticker_items;

		Message() = default;
		Message(Memory_Allocator allocator) :
			mentions(allocator), mention_roles(allocator), mention_channels(allocator),
			attachments(allocator), embeds(allocator), reactions(allocator),
			components(allocator), sticker_items(allocator) {}
	};

	//
	//
	//

	struct InteractionData {
		struct ResolvedData {
			Hash_Table<Snowflake, User>        users;
			Hash_Table<Snowflake, GuildMember> members;
			Hash_Table<Snowflake, Role>        roles;
			Hash_Table<Snowflake, Channel>     channels;
			Hash_Table<Snowflake, Message>     messages;
			Hash_Table<Snowflake, Attachment>  attachments;

			ResolvedData() = default;
			ResolvedData(Memory_Allocator allocator):
				users(allocator), members(allocator), roles(allocator),
				channels(allocator), messages(allocator), attachments(allocator) {}
		};

		Snowflake                                      id;
		String                                         name;
		ApplicationCommandType                         type = ApplicationCommandType::NONE;
		ResolvedData *                                 resolved = nullptr;
		Array<ApplicationCommandInteractionDataOption> options;
		Snowflake                                      guild_id;
		String                                         custom_id;
		ComponentType                                  component_type = ComponentType::ACTION_ROW;
		Array<SelectOption>                            values;
		Snowflake                                      target_id;
		Array<Component>                               components;

		InteractionData() = default;
		InteractionData(Memory_Allocator allocator):
			options(allocator), values(allocator), components(allocator) {}
	};

	struct Interaction {
		Snowflake        id;
		Snowflake        application_id;
		InteractionType  type = InteractionType::NONE;
		InteractionData *data = nullptr;
		Snowflake        guild_id;
		Snowflake        channel_id;
		GuildMember *    member = nullptr;
		User *           user = nullptr;
		String           token;
		int32_t          version = 0;
		Message *        message = nullptr;
		String           locale;
		String           guild_locale;
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
		GUILD_CREATE, GUILD_UPDATE, GUILD_DELETE, GUILD_BAN_ADD, GUILD_BAN_REMOVE,
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
		"GUILD_CREATE", "GUILD_UPDATE", "GUILD_DELETE", "GUILD_BAN_ADD", "GUILD_BAN_REMOVE",
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
		int32_t                 v;
		User                    user;
		Array<UnavailableGuild> guilds;
		String                  session_id;
		int32_t                 shard[2] = {};
		Application             application;

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
		Timestamp last_pin_timestamp;

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
		Snowflake    guild_id;
		ThreadMember member;

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

	struct GuildCreateEvent : public Event {
		Guild                      guild;
		Timestamp                  joined_at;
		bool                       large = false;
		bool                       unavailable = false;
		int32_t                    member_count = 0;
		Array<VoiceState>          voice_states;
		Array<GuildMember>         members;
		Array<Channel>             channels;
		Array<Channel>             threads;
		Array<Presence>            presences;
		Array<StageInstance>       stage_instances;
		Array<GuildScheduledEvent> guild_scheduled_events;

		GuildCreateEvent(): Event(EventType::GUILD_CREATE) {}
		GuildCreateEvent(Memory_Allocator allocator): 
			Event(EventType::GUILD_CREATE), guild(allocator), members(allocator),
			channels(allocator), threads(allocator), presences(allocator),
			stage_instances(allocator), guild_scheduled_events(allocator) {}
	};

	struct GuildUpdateEvent : public Event {
		Guild guild;

		GuildUpdateEvent(): Event(EventType::GUILD_UPDATE) {}
		GuildUpdateEvent(Memory_Allocator allocator):
			Event(EventType::GUILD_UPDATE), guild(allocator) {}
	};

	struct GuildDeleteEvent : public Event {
		UnavailableGuild unavailable_guild;
		GuildDeleteEvent(): Event(EventType::GUILD_DELETE) {}
	};

	struct GuildBanAddEvent : public Event {
		Snowflake guild_id;
		User      user;

		GuildBanAddEvent(): Event(EventType::GUILD_BAN_ADD) {}
	};

	struct GuildBanRemoveEvent : public Event {
		Snowflake guild_id;
		User      user;

		GuildBanRemoveEvent(): Event(EventType::GUILD_BAN_REMOVE) {}
	};

	struct GuildEmojisUpdateEvent : public Event {
		Snowflake    guild_id;
		Array<Emoji> emojis;

		GuildEmojisUpdateEvent(): Event(EventType::GUILD_EMOJIS_UPDATE) {}
		GuildEmojisUpdateEvent(Memory_Allocator allocator):
			Event(EventType::GUILD_EMOJIS_UPDATE), emojis(allocator) {}
	};

	struct GuildStickersUpdateEvent : public Event {
		Snowflake      guild_id;
		Array<Sticker> stickers;

		GuildStickersUpdateEvent() : Event(EventType::GUILD_STICKERS_UPDATE) {}
		GuildStickersUpdateEvent(Memory_Allocator allocator) :
			Event(EventType::GUILD_STICKERS_UPDATE), stickers(allocator) {}
	};

	struct GuildIntegrationsUpdateEvent : public Event {
		Snowflake guild_id;
		GuildIntegrationsUpdateEvent(): Event(EventType::GUILD_INTEGRATIONS_UPDATE) {}
	};

	struct GuildMemberAddEvent : public Event {
		Snowflake  guild_id;
		GuildMember member;

		GuildMemberAddEvent(): Event(EventType::GUILD_MEMBER_ADD) {}
		GuildMemberAddEvent(Memory_Allocator allocator):
			Event(EventType::GUILD_MEMBER_ADD), member(allocator) {}
	};

	struct GuildMemberRemoveEvent : public Event {
		Snowflake  guild_id;
		User       user;

		GuildMemberRemoveEvent(): Event(EventType::GUILD_MEMBER_REMOVE) {}
	};

	struct GuildMemberUpdateEvent : public Event {
		Snowflake        guild_id;
		Array<Snowflake> roles;
		User             user;
		String           nick;
		String           avatar;
		Timestamp        joined_at = 0;
		Timestamp        premium_since = 0;
		bool             deaf = false;
		bool             mute = false;
		bool             pending = false;
		Timestamp        communication_disabled_until = 0;

		GuildMemberUpdateEvent(): Event(EventType::GUILD_MEMBER_UPDATE) {}
		GuildMemberUpdateEvent(Memory_Allocator allocator):
			Event(EventType::GUILD_MEMBER_UPDATE), roles(allocator) {}
	};

	struct GuildMembersChunkEvent : public Event {
		Snowflake          guild_id;
		Array<GuildMember> members;
		int32_t            chunk_index = 0;
		int32_t            chunk_count = 0;
		Array<Snowflake>   not_found;
		Array<Presence>    presences;
		String             nonce;

		GuildMembersChunkEvent(): Event(EventType::GUILD_MEMBERS_CHUNK) {}
		GuildMembersChunkEvent(Memory_Allocator allocator):
			Event(EventType::GUILD_MEMBERS_CHUNK), members(allocator), not_found(allocator), presences(allocator) {}
	};

	struct GuildRoleCreateEvent : public Event {
		Snowflake guild_id;
		Role      role;
		GuildRoleCreateEvent(): Event(EventType::GUILD_ROLE_CREATE) {}
	};
	
	struct GuildRoleUpdateEvent : public Event {
		Snowflake guild_id;
		Role      role;
		GuildRoleUpdateEvent(): Event(EventType::GUILD_ROLE_UPDATE) {}
	};

	struct GuildRoleDeleteEvent : public Event {
		Snowflake guild_id;
		Snowflake role_id;
		GuildRoleDeleteEvent(): Event(EventType::GUILD_ROLE_DELETE) {}
	};

	struct GuildScheduledEventCreateEvent : public Event {
		GuildScheduledEvent scheduled_event;
		GuildScheduledEventCreateEvent(): Event(EventType::GUILD_SCHEDULED_EVENT_CREATE) {}
	};
	
	struct GuildScheduledEventUpdateEvent : public Event {
		GuildScheduledEvent scheduled_event;
		GuildScheduledEventUpdateEvent(): Event(EventType::GUILD_SCHEDULED_EVENT_UPDATE) {}
	};
	
	struct GuildScheduledEventDeleteEvent : public Event {
		GuildScheduledEvent scheduled_event;
		GuildScheduledEventDeleteEvent(): Event(EventType::GUILD_SCHEDULED_EVENT_DELETE) {}
	};

	struct GuildScheduledEventUserAddEvent : public Event {
		Snowflake guild_scheduled_event_id;
		Snowflake user_id;
		Snowflake guild_id;
		GuildScheduledEventUserAddEvent(): Event(EventType::GUILD_SCHEDULED_EVENT_USER_ADD) {}
	};

	struct GuildScheduledEventUserRemoveEvent : public Event {
		Snowflake guild_scheduled_event_id;
		Snowflake user_id;
		Snowflake guild_id;
		GuildScheduledEventUserRemoveEvent(): Event(EventType::GUILD_SCHEDULED_EVENT_USER_REMOVE) {}
	};

	struct IntegrationCreateEvent : public Event {
		Snowflake   guild_id;
		Integration integration;
		IntegrationCreateEvent(): Event(EventType::INTEGRATION_CREATE) {}
	};

	struct IntegrationUpdateEvent : public Event {
		Snowflake   guild_id;
		Integration integration;
		IntegrationUpdateEvent(): Event(EventType::INTEGRATION_UPDATE) {}
	};

	struct IntegrationDeleteEvent : public Event {
		Snowflake id;
		Snowflake guild_id;
		Snowflake application_id;
		IntegrationDeleteEvent(): Event(EventType::INTEGRATION_DELETE) {}
	};

	struct InteractionCreateEvent : public Event {
		Interaction interation;
		InteractionCreateEvent(): Event(EventType::INTERACTION_CREATE) {}
	};

	struct InviteCreateEvent : public Event {
		Snowflake        channel_id;
		String           code;
		Timestamp        created_at;
		Snowflake        guild_id;
		User *           inviter = nullptr;
		int32_t          max_age;
		int32_t          max_uses;
		InviteTargetType target_type = InviteTargetType::NONE;
		User *           target_user = nullptr;
		Application *    target_application = nullptr;
		bool             temporary = false;
		int32_t          uses = 0;

		InviteCreateEvent(): Event(EventType::INVITE_CREATE) {}
	};

	struct InviteDeleteEvent : public Event {
		Snowflake channel_id;
		Snowflake guild_id;
		String    code;

		InviteDeleteEvent(): Event(EventType::INVITE_DELETE) {}
	};

	struct MessageCreateEvent : public Event {
		Message message;
		MessageCreateEvent(): Event(EventType::MESSAGE_CREATE) {}
		MessageCreateEvent(Memory_Allocator allocator):
			Event(EventType::MESSAGE_CREATE), message(allocator) {}
	};

	struct MessageUpdateEvent : public Event {
		Message message;
		MessageUpdateEvent() : Event(EventType::MESSAGE_UPDATE) {}
		MessageUpdateEvent(Memory_Allocator allocator):
			Event(EventType::MESSAGE_UPDATE), message(allocator) {}
	};

	struct MessageDeleteEvent : public Event {
		Snowflake id;
		Snowflake channel_id;
		Snowflake guild_id;
		MessageDeleteEvent(): Event(EventType::MESSAGE_DELETE) {}
	};
	
	struct MessageDeleteBulkEvent : public Event {
		Array<Snowflake> ids;
		Snowflake        channel_id;
		Snowflake        guild_id;
		MessageDeleteBulkEvent(): Event(EventType::MESSAGE_DELETE_BULK) {}
		MessageDeleteBulkEvent(Memory_Allocator allocator):
			Event(EventType::MESSAGE_DELETE_BULK), ids(allocator) {}
	};

	struct MessageReactionAddEvent : public Event {
		Snowflake    user_id;
		Snowflake    channel_id;
		Snowflake    message_id;
		Snowflake    guild_id;
		GuildMember *member = nullptr;
		Emoji        emoji;

		MessageReactionAddEvent(): Event(EventType::MESSAGE_REACTION_ADD) {}
		MessageReactionAddEvent(Memory_Allocator allocator):
			Event(EventType::MESSAGE_REACTION_ADD), emoji(allocator) {}
	};

	struct MessageReactionRemoveEvent : public Event {
		Snowflake user_id;
		Snowflake channel_id;
		Snowflake message_id;
		Snowflake guild_id;
		Emoji     emoji;

		MessageReactionRemoveEvent(): Event(EventType::MESSAGE_REACTION_REMOVE) {}
		MessageReactionRemoveEvent(Memory_Allocator allocator):
			Event(EventType::MESSAGE_REACTION_REMOVE), emoji(allocator) {}
	};

	struct MessageReactionRemoveAllEvent : public Event {
		Snowflake channel_id;
		Snowflake message_id;
		Snowflake guild_id;

		MessageReactionRemoveAllEvent(): Event(EventType::MESSAGE_REACTION_REMOVE_ALL) {}
	};

	struct MessageReactionRemoveEmojiEvent : public Event {
		Snowflake channel_id;
		Snowflake guild_id;
		Snowflake message_id;
		Emoji     emoji;

		MessageReactionRemoveEmojiEvent(): Event(EventType::MESSAGE_REACTION_REMOVE_EMOJI) {}
		MessageReactionRemoveEmojiEvent(Memory_Allocator allocator):
			Event(EventType::MESSAGE_REACTION_REMOVE_EMOJI), emoji(allocator) {}
	};

	struct PresenceUpdateEvent : public Event {
		Presence presence;
		PresenceUpdateEvent(): Event(EventType::PRESENCE_UPDATE) {}
		PresenceUpdateEvent(Memory_Allocator allocator): 
			Event(EventType::PRESENCE_UPDATE), presence(allocator) {}
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

static Discord::Timestamp Discord_ParseTimestamp(String timestamp) {
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
	member->communication_disabled_until = Discord_ParseTimestamp(JsonGetString(obj, "communication_disabled_until"));
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

static void Discord_Deserialize(const Json_Object &obj, Discord::RoleTag *role) {
	role->bot_id         = Discord_ParseId(JsonGetString(obj, "bot_id"));
	role->integration_id = Discord_ParseId(JsonGetString(obj, "integration_id"));
	
	const Json *premium_subscriber = obj.Find("premium_subscriber");
	if (premium_subscriber) {
		if (premium_subscriber->type == JSON_TYPE_NULL)
			role->premium_subscriber = true;
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Role *role) {
	role->id            = Discord_ParseId(JsonGetString(obj, "id"));
	role->name          = JsonGetString(obj, "name");
	role->color         = JsonGetInt(obj, "color");
	role->hoist         = JsonGetBool(obj, "hoist");
	role->icon          = JsonGetString(obj, "icon");
	role->unicode_emoji = JsonGetString(obj, "unicode_emoji");
	role->position      = JsonGetInt(obj, "position");
	role->permissions   = Discord_ParseBigInt(JsonGetString(obj, "permissions"));
	role->managed       = JsonGetBool(obj, "managed");
	role->mentionable   = JsonGetBool(obj, "mentionable");
	
	const Json *tags = obj.Find("tags");
	if (tags) {
		role->tags = new Discord::RoleTag;
		if (role->tags) {
			Discord_Deserialize(JsonGetObject(*tags), role->tags);
		}
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Emoji *emoji) {
	emoji->id   = Discord_ParseId(JsonGetString(obj, "id"));
	emoji->name = JsonGetString(obj, "name");

	Json_Array roles = JsonGetArray(obj, "roles");
	emoji->roles.Resize(roles.count);
	for (ptrdiff_t index = 0; index < emoji->roles.count; ++index) {
		Discord_Deserialize(JsonGetObject(roles[index]), &emoji->roles[index]);
	}

	const Json *user = obj.Find("user");
	if (user) {
		emoji->user = new Discord::User;
		if (emoji->user) {
			Discord_Deserialize(JsonGetObject(*user), emoji->user);
		}
	}

	emoji->require_colons = JsonGetBool(obj, "require_colons");
	emoji->managed        = JsonGetBool(obj, "managed");
	emoji->animated       = JsonGetBool(obj, "animated");
	emoji->available      = JsonGetBool(obj, "available");
}

static void Discord_Deserialize(String name, Discord::GuildFeature *feature) {
	static const String GuildFeatureNames[] = {
		"ANIMATED_BANNER", "ANIMATED_ICON", "BANNER", "COMMERCE", "COMMUNITY", "DISCOVERABLE",
		"FEATURABLE", "INVITE_SPLASH", "MEMBER_VERIFICATION_GATE_ENABLED",
		"MONETIZATION_ENABLED", "MORE_STICKERS", "NEWS", "PARTNERED", "PREVIEW_ENABLED",
		"PRIVATE_THREADS", "ROLE_ICONS", "TICKETED_EVENTS_ENABLED", "VANITY_URL", 
		"VERIFIED", "VIP_REGIONS", "WELCOME_SCREEN_ENABLED",
	};
	static_assert(ArrayCount(GuildFeatureNames) == (int)Discord::GuildFeature::GUILD_FEATURE_COUNT, "");

	for (int32_t index = 0; index < ArrayCount(GuildFeatureNames); ++index) {
		if (name == GuildFeatureNames[index]) {
			*feature = (Discord::GuildFeature)index;
		}
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::WelcomeScreenChannel *welcome) {
	welcome->channel_id  = Discord_ParseId(JsonGetString(obj, "channel_id"));
	welcome->description = JsonGetString(obj, "description");
	welcome->emoji_id    = Discord_ParseId(JsonGetString(obj, "emoji_id"));
	welcome->emoji_name  = JsonGetString(obj, "emoji_name");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::WelcomeScreen *welcome) {
	welcome->description = JsonGetString(obj, "description");

	Json_Array channels = JsonGetArray(obj, "welcome_channels");
	welcome->welcome_channels.Resize(channels.count);
	for (ptrdiff_t index = 0; index < welcome->welcome_channels.count; ++index) {
		Discord_Deserialize(JsonGetObject(channels[index]), &welcome->welcome_channels[index]);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Sticker *sticker) {
	sticker->id          = Discord_ParseId(JsonGetString(obj, "id"));
	sticker->pack_id     = Discord_ParseId(JsonGetString(obj, "pack_id"));
	sticker->name        = JsonGetString(obj, "name");
	sticker->description = JsonGetString(obj, "description");
	sticker->tags        = JsonGetString(obj, "tags");
	sticker->type        = (Discord::StickerType)JsonGetInt(obj, "type");
	sticker->format_type = (Discord::StickerFormatType)JsonGetInt(obj, "format_type");
	sticker->available   = JsonGetBool(obj, "available");
	sticker->guild_id    = Discord_ParseId(JsonGetString(obj, "guild_id"));
	sticker->sort_value  = JsonGetInt(obj, "sort_value");
	
	const Json *user = obj.Find("user");
	if (user) {
		sticker->user = new Discord::User;
		if (sticker->user) {
			Discord_Deserialize(JsonGetObject(*user), sticker->user);
		}
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::UnavailableGuild *guild) {
	guild->id          = Discord_ParseId(JsonGetString(obj, "id"));
	guild->unavailable = JsonGetBool(obj, "unavailable");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Guild *guild) {
	guild->id                            = Discord_ParseId(JsonGetString(obj, "id"));
	guild->name                          = JsonGetString(obj, "name");
	guild->icon                          = JsonGetString(obj, "icon");
	guild->icon_hash                     = JsonGetString(obj, "icon_hash");
	guild->splash                        = JsonGetString(obj, "splash");
	guild->discovery_splash              = JsonGetString(obj, "discovery_splash");
	guild->owner                         = JsonGetBool(obj, "owner");
	guild->owner_id                      = Discord_ParseId(JsonGetString(obj, "owner_id"));
	guild->permissions                   = Discord_ParseBigInt(JsonGetString(obj, "permissions"));
	guild->afk_channel_id                = Discord_ParseId(JsonGetString(obj, "afk_channel_id"));
	guild->afk_timeout                   = JsonGetInt(obj, "afk_timeout");
	guild->widget_enabled                = JsonGetBool(obj, "widget_enabled");
	guild->widget_channel_id             = Discord_ParseId(JsonGetString(obj, "widget_channel_id"));
	guild->verification_level            = (Discord::VerificationLevel)JsonGetInt(obj, "verification_level");
	guild->default_message_notifications = (Discord::MessageNotificationLevel)JsonGetInt(obj, "default_message_notifications");
	guild->explicit_content_filter       = (Discord::ExplicitContentFilterLevel)JsonGetInt(obj, "explicit_content_filter");

	Json_Array roles = JsonGetArray(obj, "roles");
	guild->roles.Resize(roles.count);
	for (ptrdiff_t index = 0; index < guild->roles.count; ++index) {
		Discord_Deserialize(JsonGetObject(roles[index]), &guild->roles[index]);
	}

	Json_Array emojis = JsonGetArray(obj, "emojis");
	guild->emojis.Resize(emojis.count);
	for (ptrdiff_t index = 0; index < guild->emojis.count; ++index) {
		Discord_Deserialize(JsonGetObject(emojis[index]), &guild->emojis[index]);
	}
	
	Json_Array features = JsonGetArray(obj, "features");
	guild->features.Resize(features.count);
	for (ptrdiff_t index = 0; index < guild->features.count; ++index) {
		Discord_Deserialize(JsonGetString(features[index]), &guild->features[index]);
	}

	guild->mfa_level                  = (Discord::MFALevel)JsonGetInt(obj, "mfa_level");
	guild->application_id             = Discord_ParseId(JsonGetString(obj, "application_id"));
	guild->system_channel_id          = Discord_ParseId(JsonGetString(obj, "system_channel_id"));
	guild->system_channel_flags       = JsonGetInt(obj, "system_channel_flags");
	guild->rules_channel_id           = Discord_ParseId(JsonGetString(obj, "rules_channel_id"));
	guild->max_presences              = JsonGetInt(obj, "max_presences");
	guild->max_members                = JsonGetInt(obj, "max_members");
	guild->vanity_url_code            = JsonGetString(obj, "vanity_url_code");
	guild->description                = JsonGetString(obj, "description");
	guild->banner                     = JsonGetString(obj, "banner");
	guild->premium_tier               = (Discord::PremiumTier)JsonGetInt(obj, "premium_tier");
	guild->premium_subscription_count = JsonGetInt(obj, "premium_subscription_count");
	guild->preferred_locale           = JsonGetString(obj, "preferred_locale");
	guild->public_updates_channel_id  = Discord_ParseId(JsonGetString(obj, "public_updates_channel_id"));
	guild->max_video_channel_users    = JsonGetInt(obj, "max_video_channel_users");
	guild->approximate_member_count   = JsonGetInt(obj, "approximate_member_count");
	guild->approximate_presence_count = JsonGetInt(obj, "approximate_presence_count");

	const Json *welcome_screen = obj.Find("welcome_screen");
	if (welcome_screen) {
		guild->welcome_screen = new Discord::WelcomeScreen;
		if (guild->welcome_screen) {
			Discord_Deserialize(JsonGetObject(*welcome_screen), guild->welcome_screen);
		}
	}

	guild->nsfw_level = (Discord::GuildNSFWLevel)JsonGetInt(obj, "nsfw_level");
	guild->premium_progress_bar_enabled = JsonGetInt(obj, "premium_progress_bar_enabled");

	Json_Array stickers = JsonGetArray(obj, "stickers");
	guild->stickers.Resize(stickers.count);
	for (ptrdiff_t index = 0; index < guild->stickers.count; ++index) {
		Discord_Deserialize(JsonGetObject(stickers[index]), &guild->stickers[index]);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::VoiceState *voice) {
	voice->guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	voice->channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	voice->user_id    = Discord_ParseId(JsonGetString(obj, "user_id"));

	const Json *member = obj.Find("member");
	if (member) {
		voice->member = new Discord::GuildMember;
		if (voice->member) {
			Discord_Deserialize(JsonGetObject(*member), voice->member);
		}
	}

	voice->session_id                 = JsonGetString(obj, "session_id");
	voice->deaf                       = JsonGetBool(obj, "deaf");
	voice->mute                       = JsonGetBool(obj, "mute");
	voice->self_deaf                  = JsonGetBool(obj, "self_deaf");
	voice->self_mute                  = JsonGetBool(obj, "self_mute");
	voice->self_stream                = JsonGetBool(obj, "self_stream");
	voice->self_video                 = JsonGetBool(obj, "self_video");
	voice->suppress                   = JsonGetBool(obj, "suppress");
	voice->request_to_speak_timestamp = Discord_ParseTimestamp(JsonGetString(obj, "request_to_speak_timestamp"));
}

static void Discord_Deserialize(const Json_Object &obj, Discord::StageInstance *stage) {
	stage->id                       = Discord_ParseId(JsonGetString(obj, "id"));
	stage->guild_id                 = Discord_ParseId(JsonGetString(obj, "guild_id"));
	stage->channel_id               = Discord_ParseId(JsonGetString(obj, "channel_id"));
	stage->topic                    = JsonGetString(obj, "topic");
	stage->privacy_level            = (Discord::PrivacyLevel)JsonGetInt(obj, "privacy_level");
	stage->discoverable_disabled    = JsonGetBool(obj, "discoverable_disabled");
	stage->guild_scheduled_event_id = Discord_ParseId(JsonGetString(obj, "guild_scheduled_event_id"));
}

static void Discord_Deserialize(const Json_Object &obj, Discord::GuildScheduledEventEntityMetadata *metadata) {
	metadata->location = JsonGetString(obj, "location");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::GuildScheduledEvent *event) {
	event->id                   = Discord_ParseId(JsonGetString(obj, "id"));
	event->guild_id             = Discord_ParseId(JsonGetString(obj, "guild_id"));
	event->channel_id           = Discord_ParseId(JsonGetString(obj, "channel_id"));
	event->creator_id           = Discord_ParseId(JsonGetString(obj, "creator_id"));
	event->name                 = JsonGetString(obj, "name");
	event->description          = JsonGetString(obj, "description");
	event->scheduled_start_time = Discord_ParseTimestamp(JsonGetString(obj, "scheduled_start_time"));
	event->scheduled_end_time   = Discord_ParseTimestamp(JsonGetString(obj, "scheduled_end_time"));
	event->privacy_level        = (Discord::GuildScheduledEventPrivacyLevel)JsonGetInt(obj, "privacy_level");
	event->status               = (Discord::GuildScheduledEventStatus)JsonGetInt(obj, "status");
	event->entity_type          = (Discord::GuildScheduledEventEntityType)JsonGetInt(obj, "entity_type");
	event->entity_id            = Discord_ParseId(JsonGetString(obj, "entity_id"));
	
	const Json *metadata = obj.Find("entity_metadata");
	if (metadata) {
		event->entity_metadata = new Discord::GuildScheduledEventEntityMetadata;
		if (event->entity_metadata) {
			Discord_Deserialize(JsonGetObject(*metadata), event->entity_metadata);
		}
	}

	const Json *creator = obj.Find("creator");
	if (creator) {
		event->creator = new Discord::User;
		if (event->creator) {
			Discord_Deserialize(JsonGetObject(*creator), event->creator);
		}
	}

	event->user_count = JsonGetInt(obj, "user_count");
	event->image      = JsonGetString(obj, "image");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::IntegrationAccount *account) {
	account->id   = JsonGetString(obj, "id");
	account->name = JsonGetString(obj, "name");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::IntegrationApplication *application) {
	application->id          = Discord_ParseId(JsonGetString(obj, "id"));
	application->name        = JsonGetString(obj, "name");
	application->icon        = JsonGetString(obj, "icon");
	application->description = JsonGetString(obj, "description");

	const Json *bot = obj.Find("bot");
	if (bot) {
		application->bot = new Discord::User;
		if (application->bot) {
			Discord_Deserialize(JsonGetObject(*bot), application->bot);
		}
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Integration *integration) {
	integration->id                  = Discord_ParseId(JsonGetString(obj, "id"));
	integration->name                = JsonGetString(obj, "name");
	integration->type                = JsonGetString(obj, "type");
	integration->enabled             = JsonGetBool(obj, "enabled");
	integration->syncing             = JsonGetBool(obj, "syncing");
	integration->role_id             = Discord_ParseId(JsonGetString(obj, "role_id"));
	integration->enable_emoticons    = JsonGetBool(obj, "enable_emoticons");
	integration->expire_behavior     = (Discord::IntegrationExpireBehavior)JsonGetInt(obj, "expire_behavior");
	integration->expire_grace_period = JsonGetInt(obj, "expire_grace_period");

	const Json *user = obj.Find("user");
	if (user) {
		integration->user = new Discord::User;
		if (integration->user) {
			Discord_Deserialize(JsonGetObject(*user), integration->user);
		}
	}

	Discord_Deserialize(JsonGetObject(obj, "account"), &integration->account);

	integration->synced_at        = Discord_ParseTimestamp(JsonGetString(obj, "synced_at"));
	integration->subscriber_count = JsonGetInt(obj, "subscriber_count");
	integration->revoked          = JsonGetBool(obj, "revoked");

	const Json *application = obj.Find("application");
	if (application) {
		integration->application = new Discord::IntegrationApplication;
		if (integration->application) {
			Discord_Deserialize(JsonGetObject(*application), integration->application);
		}
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Mentions *mentions) {
	Discord_Deserialize(obj, &mentions->user);
	Discord_Deserialize(JsonGetObject(obj, "member"), &mentions->member);
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ChannelMention *mention) {
	mention->id       = Discord_ParseId(JsonGetString(obj, "id"));
	mention->guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	mention->type     = (Discord::ChannelType)JsonGetInt(obj, "type");
	mention->name     = JsonGetString(obj, "name");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Attachment *attachment) {
	attachment->id           = Discord_ParseId(JsonGetString(obj, "id"));
	attachment->filename     = JsonGetString(obj, "filename");
	attachment->description  = JsonGetString(obj, "description");
	attachment->content_type = JsonGetString(obj, "content_type");
	attachment->size         = JsonGetInt(obj, "size");
	attachment->url          = JsonGetString(obj, "url");
	attachment->proxy_url    = JsonGetString(obj, "proxy_url");
	attachment->height       = JsonGetInt(obj, "height");
	attachment->width        = JsonGetInt(obj, "width");
	attachment->ephemeral    = JsonGetBool(obj, "ephemeral");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Reaction *reaction) {
	reaction->count = JsonGetInt(obj, "count");
	reaction->me    = JsonGetBool(obj, "me");
	Discord_Deserialize(JsonGetObject(obj, "emoji"), &reaction->emoji);
}

static void Discord_Deserialize(const Json_Object &obj, Discord::MessageActivity *activity) {
	activity->type     = (Discord::MessageActivityType)JsonGetInt(obj, "type");
	activity->party_id = JsonGetString(obj, "party_id");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::TeamMember *member) {
	member->membership_state = (Discord::MembershipState)JsonGetInt(obj, "membership_state");
	member->team_id          = Discord_ParseId(JsonGetString(obj, "team_id"));

	Json_Array permissions = JsonGetArray(obj, "permissions");
	member->permissions.Resize(permissions.count);
	for (ptrdiff_t index = 0; index < member->permissions.count; ++index) {
		member->permissions[index] = JsonGetString(permissions[index]);
	}

	Discord_Deserialize(JsonGetObject(obj, "user"), &member->user);
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Team *team) {
	team->icon = JsonGetString(obj, "icon");
	team->id   = Discord_ParseId(JsonGetString(obj, "id"));

	Json_Array members = JsonGetArray(obj, "members");
	team->members.Resize(members.count);
	for (ptrdiff_t index = 0; team->members.count; ++index) {
		Discord_Deserialize(JsonGetObject(members[index]), &team->members[index]);
	}

	team->name          = JsonGetString(obj, "name");
	team->owner_user_id = Discord_ParseId(JsonGetString(obj, "owner_user_id"));
}

static void Discord_Deserialize(const Json_Object &obj, Discord::InstallParams *params) {
	Json_Array scopes = JsonGetArray(obj, "scopes");
	params->scopes.Resize(scopes.count);
	for (ptrdiff_t index = 0; index < params->scopes.count; ++index) {
		params->scopes[index] = JsonGetString(scopes[index]);
	}
	params->permissions = Discord_ParseBigInt(JsonGetString(obj, "permissions"));
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Application *application) {
	application->id          = Discord_ParseId(JsonGetString(obj, "id"));
	application->name        = JsonGetString(obj, "name");
	application->icon        = JsonGetString(obj, "icon");
	application->description = JsonGetString(obj, "description");

	Json_Array rpc_origins = JsonGetArray(obj, "rpc_origins");
	application->rpc_origins.Resize(rpc_origins.count);
	for (ptrdiff_t index = 0; index < application->rpc_origins.count; ++index) {
		application->rpc_origins[index] = JsonGetString(rpc_origins[index]);
	}

	application->bot_public             = JsonGetBool(obj, "bot_public");
	application->bot_require_code_grant = JsonGetBool(obj, "bot_require_code_grant");
	application->terms_of_service_url   = JsonGetString(obj, "terms_of_service_url");
	application->privacy_policy_url     = JsonGetString(obj, "privacy_policy_url");

	const Json *owner = obj.Find("owner");
	if (owner) {
		application->owner = new Discord::User;
		if (application->owner) {
			Discord_Deserialize(JsonGetObject(*owner), application->owner);
		}
	}

	application->verify_key = JsonGetString(obj, "verify_key");

	const Json *team = obj.Find("team");
	if (team) {
		application->team = new Discord::Team;
		if (application->team) {
			Discord_Deserialize(JsonGetObject(*team), application->team);
		}
	}

	application->guild_id       = Discord_ParseId(JsonGetString(obj, "guild_id"));
	application->primary_sku_id = Discord_ParseId(JsonGetString(obj, "primary_sku_id"));
	application->slug           = JsonGetString(obj, "slug");
	application->cover_image    = JsonGetString(obj, "cover_image");
	application->flags          = JsonGetInt(obj, "flags");

	Json_Array tags      = JsonGetArray(obj, "tags");
	ptrdiff_t tags_count = Minimum(tags.count, ArrayCount(application->tags));
	for (ptrdiff_t index = 0; index < tags_count; ++index) {
		application->tags[index] = JsonGetString(tags[index]);
	}

	const Json *install_params = obj.Find("install_params");
	if (install_params) {
		application->install_params = new Discord::InstallParams;
		if (application->install_params) {
			Discord_Deserialize(JsonGetObject(*install_params), application->install_params);
		}
	}

	application->custom_install_url = JsonGetString(obj, "custom_install_url");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::MessageReference *reference) {
	reference->message_id         = Discord_ParseId(JsonGetString(obj, "message_id"));
	reference->channel_id         = Discord_ParseId(JsonGetString(obj, "channel_id"));
	reference->guild_id           = Discord_ParseId(JsonGetString(obj, "guild_id"));
	reference->fail_if_not_exists = JsonGetBool(obj, "fail_if_not_exists");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::MessageInteraction *interaction) {
	interaction->id   = Discord_ParseId(JsonGetString(obj, "id"));
	interaction->type = (Discord::InteractionType)JsonGetInt(obj, "type");
	interaction->name = JsonGetString(obj, "name");

	Discord_Deserialize(JsonGetObject(obj, "user"), &interaction->user);

	const Json *member = obj.Find("member");
	if (member) {
		interaction->member = new Discord::GuildMember;
		if (interaction->member) {
			Discord_Deserialize(JsonGetObject(*member), interaction->member);
		}
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Component *component);

static void Discord_Deserialize(const Json_Object &obj, Discord::Component::ActionRow *action_row) {
	Json_Array components = JsonGetArray(obj, "components");
	action_row->components.Resize(components.count);
	ptrdiff_t index = 0;
	for (; index < action_row->components.count; ++index) {
		action_row->components[index] = new Discord::Component;
		if (!action_row->components[index]) break;
		Discord_Deserialize(JsonGetObject(components[index]), action_row->components[index]);
	}
	action_row->components.count = index;
	action_row->components.Pack();
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Component::Button *button) {
	button->style = (Discord::ButtonStyle)JsonGetInt(obj, "style");
	button->label = JsonGetString(obj, "label");

	const Json *emoji = obj.Find("emoji");
	if (emoji) {
		button->emoji = new Discord::Emoji;
		if (button->emoji) {
			Discord_Deserialize(JsonGetObject(obj, "emoji"), button->emoji);
		}
	}

	button->custom_id = JsonGetString(obj, "custom_id");
	button->url       = JsonGetString(obj, "url");
	button->disabled  = JsonGetBool(obj, "disabled");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::SelectOption *option) {
	option->label       = JsonGetString(obj, "label");
	option->value       = JsonGetString(obj, "value");
	option->description = JsonGetString(obj, "description");

	const Json *emoji = obj.Find("emoji");
	if (emoji) {
		option->emoji = new Discord::Emoji;
		Discord_Deserialize(JsonGetObject(*emoji), option->emoji);
	}

	option->isdefault = JsonGetBool(obj, "default");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Component::SelectMenu *menu) {
	menu->custom_id = JsonGetString(obj, "custom_id");

	Json_Array options = JsonGetArray(obj, "options");
	menu->options.Resize(options.count);
	for (ptrdiff_t index = 0; index < menu->options.count; ++index) {
		Discord_Deserialize(JsonGetObject(options[index]), &menu->options[index]);
	}

	menu->placeholder = JsonGetString(obj, "placeholder");
	menu->min_values  = JsonGetInt(obj, "min_values");
	menu->max_values  = JsonGetInt(obj, "max_values");
	menu->disabled    = JsonGetBool(obj, "disabled");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Component::TextInput *text_input) {
	text_input->custom_id   = JsonGetString(obj, "custom_id");
	text_input->style       = (Discord::TextInputStyle)JsonGetInt(obj, "style");
	text_input->label       = JsonGetString(obj, "label");
	text_input->min_length  = JsonGetInt(obj, "min_length");
	text_input->max_length  = JsonGetInt(obj, "max_length");
	text_input->required    = JsonGetBool(obj, "required");
	text_input->value       = JsonGetString(obj, "value");
	text_input->placeholder = JsonGetString(obj, "placeholder");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Component *component) {
	component->type = (Discord::ComponentType)JsonGetInt(obj, "type");

	if (component->type == Discord::ComponentType::ACTION_ROW) {
		component->data.action_row = Discord::Component::ActionRow();
		Discord_Deserialize(obj, &component->data.action_row);
	} else if (component->type == Discord::ComponentType::BUTTON) {
		component->data.button = Discord::Component::Button();
		Discord_Deserialize(obj, &component->data.button);
	} else if (component->type == Discord::ComponentType::SELECT_MENU) {
		component->data.select_menu = Discord::Component::SelectMenu();
		Discord_Deserialize(obj, &component->data.select_menu);
	} else if (component->type == Discord::ComponentType::TEXT_INPUT) {
		component->data.text_input = Discord::Component::TextInput();
		Discord_Deserialize(obj, &component->data.text_input);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::StickerItem *sticker) {
	sticker->id          = Discord_ParseId(JsonGetString(obj, "id"));
	sticker->name        = JsonGetString(obj, "name");
	sticker->format_type = (Discord::StickerFormatType)JsonGetInt(obj, "format_type");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::EmbedFooter *embed) {
	embed->text           = JsonGetString(obj, "text");
	embed->icon_url       = JsonGetString(obj, "icon_url");
	embed->proxy_icon_url = JsonGetString(obj, "proxy_icon_url");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::EmbedImage *embed) {
	embed->url       = JsonGetString(obj, "url");
	embed->proxy_url = JsonGetString(obj, "proxy_url");
	embed->height    = JsonGetInt(obj, "height");
	embed->width     = JsonGetInt(obj, "width");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::EmbedThumbnail *embed) {
	embed->url       = JsonGetString(obj, "url");
	embed->proxy_url = JsonGetString(obj, "proxy_url");
	embed->height    = JsonGetInt(obj, "height");
	embed->width     = JsonGetInt(obj, "width");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::EmbedVideo *embed) {
	embed->url       = JsonGetString(obj, "url");
	embed->proxy_url = JsonGetString(obj, "proxy_url");
	embed->height    = JsonGetInt(obj, "height");
	embed->width     = JsonGetInt(obj, "width");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::EmbedProvider *embed) {
	embed->name = JsonGetString(obj, "name");
	embed->url  = JsonGetString(obj, "url");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::EmbedAuthor *embed) {
	embed->name           = JsonGetString(obj, "name");
	embed->url            = JsonGetString(obj, "url");
	embed->icon_url       = JsonGetString(obj, "icon_url");
	embed->proxy_icon_url = JsonGetString(obj, "proxy_icon_url");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::EmbedField *embed) {
	embed->name     = JsonGetString(obj, "name");
	embed->value    = JsonGetString(obj, "value");
	embed->isinline = JsonGetBool(obj, "inline");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Embed *embed) {
	embed->title       = JsonGetString(obj, "title");
	embed->type        = JsonGetString(obj, "type");
	embed->description = JsonGetString(obj, "description");
	embed->url         = JsonGetString(obj, "url");
	embed->timestamp   = Discord_ParseTimestamp(JsonGetString(obj, "timestamp"));
	embed->color       = JsonGetInt(obj, "color");

	const Json *footer = obj.Find("footer");
	if (footer) {
		embed->footer = new Discord::EmbedFooter;
		if (embed->footer) {
			Discord_Deserialize(JsonGetObject(*footer), embed->footer);
		}
	}
	
	const Json *image = obj.Find("image");
	if (image) {
		embed->image = new Discord::EmbedImage;
		if (embed->image) {
			Discord_Deserialize(JsonGetObject(*image), embed->image);
		}
	}

	const Json *thumbnail = obj.Find("thumbnail");
	if (thumbnail) {
		embed->thumbnail = new Discord::EmbedThumbnail;
		if (embed->thumbnail) {
			Discord_Deserialize(JsonGetObject(*thumbnail), embed->thumbnail);
		}
	}

	const Json *video = obj.Find("video");
	if (video) {
		embed->video = new Discord::EmbedVideo;
		if (embed->video) {
			Discord_Deserialize(JsonGetObject(*video), embed->video);
		}
	}
	
	const Json *provider = obj.Find("provider");
	if (provider) {
		embed->provider = new Discord::EmbedProvider;
		if (embed->provider) {
			Discord_Deserialize(JsonGetObject(*provider), embed->provider);
		}
	}

	const Json *author = obj.Find("author");
	if (author) {
		embed->author = new Discord::EmbedAuthor;
		if (embed->author) {
			Discord_Deserialize(JsonGetObject(*author), embed->author);
		}
	}

	Json_Array fields = JsonGetArray(obj, "fields");
	embed->fields.Resize(fields.count);
	for (ptrdiff_t index = 0; index < embed->fields.count; ++index) {
		Discord_Deserialize(JsonGetObject(fields[index]), &embed->fields[index]);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Message *message) {
	message->id         = Discord_ParseId(JsonGetString(obj, "id"));
	message->channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	message->guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Discord_Deserialize(JsonGetObject(obj, "author"), &message->author);

	const Json *member = obj.Find("member");
	if (member) {
		message->member = new Discord::GuildMember;
		if (message->member) {
			Discord_Deserialize(JsonGetObject(*member), message->member);
		}
	}

	message->content = JsonGetString(obj, "content");
	message->timestamp = Discord_ParseTimestamp(JsonGetString(obj, "timestamp"));
	message->edited_timestamp = Discord_ParseTimestamp(JsonGetString(obj, "edited_timestamp"));
	message->tts = JsonGetBool(obj, "tts");
	message->mention_everyone = JsonGetBool(obj, "mention_everyone");

	Json_Array mentions = JsonGetArray(obj, "mentions");
	message->mentions.Resize(mentions.count);
	for (ptrdiff_t index = 0; index < message->mentions.count; ++index) {
		Discord_Deserialize(JsonGetObject(mentions[0]), &message->mentions[index]);
	}

	Json_Array mention_roles = JsonGetArray(obj, "mention_roles");
	message->mention_roles.Resize(mention_roles.count);
	for (ptrdiff_t index = 0; index < message->mention_roles.count; ++index) {
		message->mention_roles[index] = Discord_ParseId(JsonGetString(mention_roles[index]));
	}

	Json_Array mention_channels = JsonGetArray(obj, "mention_channels");
	message->mention_channels.Resize(mention_channels.count);
	for (ptrdiff_t index = 0; index < message->mention_channels.count; ++index) {
		Discord_Deserialize(JsonGetObject(mention_channels[0]), &message->mention_channels[index]);
	}

	Json_Array attachments = JsonGetArray(obj, "attachments");
	message->attachments.Resize(attachments.count);
	for (ptrdiff_t index = 0; index < message->attachments.count; ++index) {
		Discord_Deserialize(JsonGetObject(attachments[0]), &message->attachments[index]);
	}

	Json_Array embeds = JsonGetArray(obj, "embeds");
	message->embeds.Resize(embeds.count);
	for (ptrdiff_t index = 0; index < message->embeds.count; ++index) {
		Discord_Deserialize(JsonGetObject(embeds[0]), &message->embeds[index]);
	}

	Json_Array reactions = JsonGetArray(obj, "reactions");
	message->reactions.Resize(reactions.count);
	for (ptrdiff_t index = 0; index < message->reactions.count; ++index) {
		Discord_Deserialize(JsonGetObject(reactions[0]), &message->reactions[index]);
	}

	message->nonce = JsonGetString(obj, "nonce");
	message->pinned = JsonGetBool(obj, "pinned");
	message->webhook_id = Discord_ParseId(JsonGetString(obj, "webhook_id"));
	message->type = (Discord::MessageType)JsonGetInt(obj, "type");

	const Json *activity = obj.Find("activity");
	if (activity) {
		message->activity = new Discord::MessageActivity;
		if (message->activity) {
			Discord_Deserialize(JsonGetObject(*activity), message->activity);
		}
	}

	const Json *application = obj.Find("application");
	if (application) {
		message->application = new Discord::Application;
		if (message->application) {
			Discord_Deserialize(JsonGetObject(*application), message->application);
		}
	}

	message->application_id = Discord_ParseId(JsonGetString(obj, "application_id"));

	const Json *message_reference = obj.Find("message_reference");
	if (message_reference) {
		message->message_reference = new Discord::MessageReference;
		if (message->message_reference) {
			Discord_Deserialize(JsonGetObject(*message_reference), message->message_reference);
		}
	}

	message->flags = JsonGetInt(obj, "flags");

	const Json *referenced_message = obj.Find("referenced_message");
	if (referenced_message && referenced_message->type == JSON_TYPE_OBJECT) {
		message->referenced_message = new Discord::Message;
		if (message->referenced_message) {
			Discord_Deserialize(referenced_message->value.object, message->referenced_message);
		}
	}

	const Json *interaction = obj.Find("interaction");
	if (interaction) {
		message->interaction = new Discord::MessageInteraction;
		if (message->interaction) {
			Discord_Deserialize(JsonGetObject(*interaction), message->interaction);
		}
	}

	const Json *thread = obj.Find("thread");
	if (thread) {
		message->thread = new Discord::Channel;
		if (message->thread) {
			Discord_Deserialize(JsonGetObject(*thread), message->thread);
		}
	}

	Json_Array components = JsonGetArray(obj, "components");
	message->components.Resize(components.count);
	for (ptrdiff_t index = 0; index < message->components.count; ++index) {
		Discord_Deserialize(JsonGetObject(components[index]), &message->components[index]);
	}

	Json_Array sticker_items = JsonGetArray(obj, "sticker_items");
	message->sticker_items.Resize(sticker_items.count);
	for (ptrdiff_t index = 0; index < message->sticker_items.count; ++index) {
		Discord_Deserialize(JsonGetObject(sticker_items[index]), &message->sticker_items[index]);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::InteractionData::ResolvedData *data) {
	Json_Object users = JsonGetObject(obj, "users");
	data->users.Resize(users.p2allocated);
	data->users.storage.Reserve(users.storage.count);
	for (auto &user_map : users) {
		Discord::Snowflake id = Discord_ParseId(user_map.key);
		Discord::User user;
		Discord_Deserialize(JsonGetObject(user_map.value), &user);
		data->users.Put(id, user);
	}

	Json_Object members = JsonGetObject(obj, "members");
	data->members.Resize(members.p2allocated);
	data->members.storage.Reserve(members.storage.count);
	for (auto &member_map : members) {
		Discord::Snowflake id = Discord_ParseId(member_map.key);
		Discord::GuildMember member;
		Discord_Deserialize(JsonGetObject(member_map.value), &member);
		data->members.Put(id, member);
	}

	Json_Object roles = JsonGetObject(obj, "roles");
	data->roles.Resize(roles.p2allocated);
	data->roles.storage.Reserve(roles.storage.count);
	for (auto &role_map : roles) {
		Discord::Snowflake id = Discord_ParseId(role_map.key);
		Discord::Role role;
		Discord_Deserialize(JsonGetObject(role_map.value), &role);
		data->roles.Put(id, role);
	}

	Json_Object channels = JsonGetObject(obj, "channels");
	data->channels.Resize(channels.p2allocated);
	data->channels.storage.Reserve(channels.storage.count);
	for (auto &channel_map : channels) {
		Discord::Snowflake id = Discord_ParseId(channel_map.key);
		Discord::Channel channel;
		Discord_Deserialize(JsonGetObject(channel_map.value), &channel);
		data->channels.Put(id, channel);
	}

	Json_Object messages = JsonGetObject(obj, "messages");
	data->messages.Resize(messages.p2allocated);
	data->messages.storage.Reserve(messages.storage.count);
	for (auto &message_map : messages) {
		Discord::Snowflake id = Discord_ParseId(message_map.key);
		Discord::Message message;
		Discord_Deserialize(JsonGetObject(message_map.value), &message);
		data->messages.Put(id, message);
	}

	Json_Object attachments = JsonGetObject(obj, "attachments");
	data->attachments.Resize(attachments.p2allocated);
	data->attachments.storage.Reserve(attachments.storage.count);
	for (auto &attachment_map : attachments) {
		Discord::Snowflake id = Discord_ParseId(attachment_map.key);
		Discord::Attachment attachment;
		Discord_Deserialize(JsonGetObject(attachment_map.value), &attachment);
		data->attachments.Put(id, attachment);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::ApplicationCommandInteractionDataOption *option) {
	option->name = JsonGetString(obj, "name");
	option->type = (Discord::ApplicationCommandOptionType)JsonGetInt(obj, "type");

	const Json *value = obj.Find("value");
	if (value) {
		if (value->type == JSON_TYPE_STRING) {
			option->value.string = value->value.string;
		} else if (value->type == JSON_TYPE_NUMBER) {
			option->value.number = value->value.number;
		} else if (value->type == JSON_TYPE_BOOL) {
			option->value.integer = value->value.boolean;
		}
	}

	Json_Array options = JsonGetArray(obj, "options");
	option->options.Resize(options.count);
	for (ptrdiff_t index = 0; index < option->options.count; ++index) {
		Discord_Deserialize(JsonGetObject(options[index]), &option->options[index]);
	}

	option->focused = JsonGetBool(obj, "focused");
}

static void Discord_Deserialize(const Json_Object &obj, Discord::InteractionData *data) {
	data->id   = Discord_ParseId(JsonGetString(obj, "id"));
	data->name = JsonGetString(obj, "name");
	data->type = (Discord::ApplicationCommandType)JsonGetInt(obj, "type");

	const Json *resolved = obj.Find("resolved");
	if (resolved) {
		data->resolved = new Discord::InteractionData::ResolvedData;
		if (data->resolved) {
			Discord_Deserialize(JsonGetObject(obj, "resolved"), data->resolved);
		}
	}

	Json_Array options = JsonGetArray(obj, "options");
	data->options.Resize(options.count);
	for (ptrdiff_t index = 0; index < data->options.count; ++index) {
		Discord_Deserialize(JsonGetObject(options[index]), &data->options[index]);
	}

	data->guild_id       = Discord_ParseId(JsonGetString(obj, "guild_id"));
	data->custom_id      = JsonGetString(obj, "custom_id");
	data->component_type = (Discord::ComponentType)JsonGetInt(obj, "component_type");

	Json_Array values = JsonGetArray(obj, "values");
	data->values.Resize(values.count);
	for (ptrdiff_t index = 0; index < data->values.count; ++index) {
		Discord_Deserialize(JsonGetObject(values[index]), &data->values[index]);
	}

	data->target_id = Discord_ParseId(JsonGetString(obj, "target_id"));

	Json_Array components = JsonGetArray(obj, "components");
	data->components.Resize(components.count);
	for (ptrdiff_t index = 0; index < data->components.count; ++index) {
		Discord_Deserialize(JsonGetObject(components[index]), &data->components[index]);
	}
}

static void Discord_Deserialize(const Json_Object &obj, Discord::Interaction *interaction) {
	interaction->id             = Discord_ParseId(JsonGetString(obj, "id"));
	interaction->application_id = Discord_ParseId(JsonGetString(obj, "application_id"));
	interaction->type           = (Discord::InteractionType)JsonGetInt(obj, "type");
	
	const Json *data = obj.Find("data");
	if (data) {
		interaction->data = new Discord::InteractionData;
		if (interaction->data) {
			Discord_Deserialize(JsonGetObject(*data), interaction->data);
		}
	}

	interaction->guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	interaction->channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));

	const Json *member = obj.Find("member");
	if (member) {
		interaction->member = new Discord::GuildMember;
		if (interaction->member) {
			Discord_Deserialize(JsonGetObject(*member), interaction->member);
		}
	}

	const Json *user = obj.Find("user");
	if (user) {
		interaction->user = new Discord::User;
		if (interaction->user) {
			Discord_Deserialize(JsonGetObject(*user), interaction->user);
		}
	}

	interaction->token   = JsonGetString(obj, "token");
	interaction->version = JsonGetInt(obj, "version");

	const Json *message = obj.Find("message");
	if (message) {
		interaction->message = new Discord::Message;
		if (interaction->message) {
			Discord_Deserialize(JsonGetObject(*message), interaction->message);
		}
	}

	interaction->locale       = JsonGetString(obj, "locale");
	interaction->guild_locale = JsonGetString(obj, "guild_locale");
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
		Discord_Deserialize(JsonGetObject(guilds[index]), &ready.guilds[index]);
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

static void Discord_EventHandlerGuildCreate(Discord::Client *client, const Json &data) {
	Discord::GuildCreateEvent guild;
	Json_Object obj = JsonGetObject(data);
	Discord_Deserialize(obj, &guild.guild);
	guild.joined_at    = Discord_ParseTimestamp(JsonGetString(obj, "joined_at"));
	guild.large        = JsonGetBool(obj, "large");
	guild.unavailable  = JsonGetBool(obj, "unavailable");
	guild.member_count = JsonGetInt(obj, "member_count");

	Json_Array voice_states = JsonGetArray(obj, "voice_states");
	guild.voice_states.Resize(voice_states.count);
	for (ptrdiff_t index = 0; index < guild.voice_states.count; ++index) {
		Discord_Deserialize(JsonGetObject(voice_states[index]), &guild.voice_states[index]);
	}

	Json_Array members = JsonGetArray(obj, "members");
	guild.members.Resize(members.count);
	for (ptrdiff_t index = 0; index < guild.members.count; ++index) {
		Discord_Deserialize(JsonGetObject(members[index]), &guild.members[index]);
	}

	Json_Array channels = JsonGetArray(obj, "channels");
	guild.channels.Resize(channels.count);
	for (ptrdiff_t index = 0; index < guild.channels.count; ++index) {
		Discord_Deserialize(JsonGetObject(channels[index]), &guild.channels[index]);
	}
	
	Json_Array threads = JsonGetArray(obj, "threads");
	guild.threads.Resize(threads.count);
	for (ptrdiff_t index = 0; index < guild.threads.count; ++index) {
		Discord_Deserialize(JsonGetObject(threads[index]), &guild.threads[index]);
	}

	Json_Array presences = JsonGetArray(obj, "presences");
	guild.presences.Resize(presences.count);
	for (ptrdiff_t index = 0; index < guild.presences.count; ++index) {
		Discord_Deserialize(JsonGetObject(presences[index]), &guild.presences[index]);
	}

	Json_Array stage_instances = JsonGetArray(obj, "stage_instances");
	guild.stage_instances.Resize(stage_instances.count);
	for (ptrdiff_t index = 0; index < guild.stage_instances.count; ++index) {
		Discord_Deserialize(JsonGetObject(stage_instances[index]), &guild.stage_instances[index]);
	}
	
	Json_Array guild_scheduled_events = JsonGetArray(obj, "guild_scheduled_events");
	guild.guild_scheduled_events.Resize(guild_scheduled_events.count);
	for (ptrdiff_t index = 0; index < guild.guild_scheduled_events.count; ++index) {
		Discord_Deserialize(JsonGetObject(guild_scheduled_events[index]), &guild.guild_scheduled_events[index]);
	}

	client->onevent(client, &guild);
}

static void Discord_EventHandlerGuildUpdate(Discord::Client *client, const Json &data) {
	Discord::GuildUpdateEvent guild;
	Discord_Deserialize(JsonGetObject(data), &guild.guild);
	client->onevent(client, &guild);
}

static void Discord_EventHandlerGuildDelete(Discord::Client *client, const Json &data) {
	Discord::GuildDeleteEvent guild;
	Discord_Deserialize(JsonGetObject(data), &guild.unavailable_guild);
	client->onevent(client, &guild);
}

static void Discord_EventHandlerGuildBanAdd(Discord::Client *client, const Json &data) {
	Discord::GuildBanAddEvent ban;
	Json_Object obj = JsonGetObject(data);
	ban.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(JsonGetObject(obj, "user"), &ban.user);
	client->onevent(client, &ban);
}

static void Discord_EventHandlerGuildBanRemove(Discord::Client *client, const Json &data) {
	Discord::GuildBanRemoveEvent ban;
	Json_Object obj = JsonGetObject(data);
	ban.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(JsonGetObject(obj, "user"), &ban.user);
	client->onevent(client, &ban);
}

static void Discord_EventHandlerGuildEmojisUpdate(Discord::Client *client, const Json &data) {
	Discord::GuildEmojisUpdateEvent emojis_update;
	Json_Object obj        = JsonGetObject(data);
	emojis_update.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	
	Json_Array emojis = JsonGetArray(obj, "emojis");
	emojis_update.emojis.Resize(emojis.count);
	for (ptrdiff_t index = 0; index < emojis_update.emojis.count; ++index) {
		Discord_Deserialize(JsonGetObject(emojis[index]), &emojis_update.emojis[index]);
	}

	client->onevent(client, &emojis_update);
}

static void Discord_EventHandlerGuildStickersUpdate(Discord::Client *client, const Json &data) {
	Discord::GuildStickersUpdateEvent stickers_update;
	Json_Object obj          = JsonGetObject(data);
	stickers_update.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Json_Array stickers = JsonGetArray(obj, "stickers");
	stickers_update.stickers.Resize(stickers.count);
	for (ptrdiff_t index = 0; index < stickers_update.stickers.count; ++index) {
		Discord_Deserialize(JsonGetObject(stickers[index]), &stickers_update.stickers[index]);
	}

	client->onevent(client, &stickers_update);
}

static void Discord_EventHandlerGuildIntegrationsUpdate(Discord::Client *client, const Json &data) {
	Discord::GuildIntegrationsUpdateEvent integrations;
	Json_Object obj       = JsonGetObject(data);
	integrations.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent(client, &integrations);
}

static void Discord_EventHandlerGuildMemberAdd(Discord::Client *client, const Json &data) {
	Discord::GuildMemberAddEvent member;
	Json_Object obj = JsonGetObject(data);
	member.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(obj, &member.member);
	client->onevent(client, &member);
}

static void Discord_EventHandlerGuildMemberRemove(Discord::Client *client, const Json &data) {
	Discord::GuildMemberRemoveEvent member;
	Json_Object obj = JsonGetObject(data);
	member.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(JsonGetObject(obj, "user"), &member.user);
	client->onevent(client, &member);
}

static void Discord_EventHandlerGuildMemberUpdate(Discord::Client *client, const Json &data) {
	Discord::GuildMemberUpdateEvent member;
	Json_Object obj = JsonGetObject(data);
	member.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Json_Array roles = JsonGetArray(obj, "roles");
	member.roles.Resize(roles.count);
	for (ptrdiff_t index = 0; index < member.roles.count; ++index) {
		member.roles[index] = Discord_ParseId(JsonGetString(roles[index]));
	}

	Discord_Deserialize(JsonGetObject(obj, "user"), &member.user);

	member.nick                         = JsonGetString(obj, "nick");
	member.avatar                       = JsonGetString(obj, "avatar");
	member.joined_at                    = Discord_ParseTimestamp(JsonGetString(obj, "joined_at"));
	member.premium_since                = Discord_ParseTimestamp(JsonGetString(obj, "premium_since"));
	member.deaf                         = JsonGetBool(obj, "deaf");
	member.mute                         = JsonGetBool(obj, "mute");
	member.pending                      = JsonGetBool(obj, "pending");
	member.communication_disabled_until = Discord_ParseTimestamp(JsonGetString(obj, "communication_disabled_until"));

	client->onevent(client, &member);
}

static void Discord_EventHandlerGuildMembersChunk(Discord::Client *client, const Json &data) {
	Discord::GuildMembersChunkEvent chunk;
	Json_Object obj = JsonGetObject(data);
	chunk.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Json_Array members = JsonGetArray(obj, "members");
	chunk.members.Resize(members.count);
	for (ptrdiff_t index = 0; index < chunk.members.count; ++index) {
		Discord_Deserialize(JsonGetObject(members[index]), &chunk.members[index]);
	}

	chunk.chunk_index = JsonGetInt(obj, "chunk_index");
	chunk.chunk_count = JsonGetInt(obj, "chunk_count");

	Json_Array not_found = JsonGetArray(obj, "not_found");
	chunk.not_found.Resize(not_found.count);
	for (ptrdiff_t index = 0; index < chunk.not_found.count; ++index) {
		chunk.not_found[index] = Discord_ParseId(JsonGetString(not_found[index]));
	}

	Json_Array presences = JsonGetArray(obj, "presences");
	chunk.presences.Resize(presences.count);
	for (ptrdiff_t index = 0; index < chunk.presences.count; ++index) {
		Discord_Deserialize(JsonGetObject(presences[index]), &chunk.presences[index]);
	}

	chunk.nonce = JsonGetString(obj, "nonce");

	client->onevent(client, &chunk);
}

static void Discord_EventHandlerGuildRoleCreate(Discord::Client *client, const Json &data) {
	Discord::GuildRoleCreateEvent role;
	Json_Object obj = JsonGetObject(data);
	role.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(JsonGetObject(obj, "role"), &role.role);
	client->onevent(client, &role);
}

static void Discord_EventHandlerGuildRoleUpdate(Discord::Client *client, const Json &data) {
	Discord::GuildRoleUpdateEvent role;
	Json_Object obj = JsonGetObject(data);
	role.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(JsonGetObject(obj, "role"), &role.role);
	client->onevent(client, &role);
}

static void Discord_EventHandlerGuildRoleDelete(Discord::Client *client, const Json &data) {
	Discord::GuildRoleDeleteEvent role;
	Json_Object obj = JsonGetObject(data);
	role.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	role.role_id    = Discord_ParseId(JsonGetString(obj, "role_id"));
	client->onevent(client, &role);
}

static void Discord_EventHandlerGuildScheduledEventCreate(Discord::Client *client, const Json &data) {
	Discord::GuildScheduledEventCreateEvent scheduled_event;
	Discord_Deserialize(JsonGetObject(data), &scheduled_event.scheduled_event);
	client->onevent(client, &scheduled_event);
}

static void Discord_EventHandlerGuildScheduledEventUpdate(Discord::Client *client, const Json &data) {
	Discord::GuildScheduledEventUpdateEvent scheduled_event;
	Discord_Deserialize(JsonGetObject(data), &scheduled_event.scheduled_event);
	client->onevent(client, &scheduled_event);
}

static void Discord_EventHandlerGuildScheduledEventDelete(Discord::Client *client, const Json &data) {
	Discord::GuildScheduledEventDeleteEvent scheduled_event;
	Discord_Deserialize(JsonGetObject(data), &scheduled_event.scheduled_event);
	client->onevent(client, &scheduled_event);
}

static void Discord_EventHandlerGuildScheduledEventUserAdd(Discord::Client *client, const Json &data) {
	Discord::GuildScheduledEventUserAddEvent scheduled_event;
	Json_Object obj                          = JsonGetObject(data);
	scheduled_event.guild_scheduled_event_id = Discord_ParseId(JsonGetString(obj, "guild_scheduled_event_id"));
	scheduled_event.user_id                  = Discord_ParseId(JsonGetString(obj, "user_id"));
	scheduled_event.guild_id                 = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent(client, &scheduled_event);
}

static void Discord_EventHandlerGuildScheduledEventUserRemove(Discord::Client *client, const Json &data) {
	Discord::GuildScheduledEventUserRemoveEvent scheduled_event;
	Json_Object obj                          = JsonGetObject(data);
	scheduled_event.guild_scheduled_event_id = Discord_ParseId(JsonGetString(obj, "guild_scheduled_event_id"));
	scheduled_event.user_id                  = Discord_ParseId(JsonGetString(obj, "user_id"));
	scheduled_event.guild_id                 = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent(client, &scheduled_event);
}

static void Discord_EventHandlerIntegrationCreate(Discord::Client *client, const Json &data) {
	Discord::IntegrationCreateEvent integration;
	Json_Object obj = JsonGetObject(data);
	integration.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(obj, &integration.integration);
	client->onevent(client, &integration);
}

static void Discord_EventHandlerIntegrationUpdate(Discord::Client *client, const Json &data) {
	Discord::IntegrationUpdateEvent integration;
	Json_Object obj = JsonGetObject(data);
	integration.guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(obj, &integration.integration);
	client->onevent(client, &integration);
}

static void Discord_EventHandlerIntegrationDelete(Discord::Client *client, const Json &data) {
	Discord::IntegrationDeleteEvent integration;
	Json_Object obj            = JsonGetObject(data);
	integration.id             = Discord_ParseId(JsonGetString(obj, "id"));
	integration.guild_id       = Discord_ParseId(JsonGetString(obj, "guild_id"));
	integration.application_id = Discord_ParseId(JsonGetString(obj, "application_id"));
	client->onevent(client, &integration);
}

static void Discord_EventHandlerInteractionCreate(Discord::Client *client, const Json &data) {
	Discord::InteractionCreateEvent interation;
	Discord_Deserialize(JsonGetObject(data), &interation.interation);
	client->onevent(client, &interation);
}

static void Discord_EventHandlerInviteCreate(Discord::Client *client, const Json &data) {
	Discord::InviteCreateEvent invite;
	Json_Object obj   = JsonGetObject(data);
	invite.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	invite.code       = JsonGetString(obj, "code");
	invite.created_at = Discord_ParseTimestamp(JsonGetString(obj, "created_at"));
	invite.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));

	const Json *inviter = obj.Find("inviter");
	if (inviter) {
		invite.inviter = new Discord::User;
		if (invite.inviter) {
			Discord_Deserialize(JsonGetObject(*inviter), invite.inviter);
		}
	}

	invite.max_age     = JsonGetInt(obj, "max_age");
	invite.max_uses    = JsonGetInt(obj, "max_uses");
	invite.target_type = (Discord::InviteTargetType)JsonGetInt(obj, "target_type");

	const Json *target_user = obj.Find("target_user");
	if (target_user) {
		invite.target_user = new Discord::User;
		if (invite.target_user) {
			Discord_Deserialize(JsonGetObject(*target_user), invite.target_user);
		}
	}

	const Json *target_application = obj.Find("target_application");
	if (target_application) {
		invite.target_application = new Discord::Application;
		if (invite.target_application) {
			Discord_Deserialize(JsonGetObject(*target_application), invite.target_application);
		}
	}

	invite.temporary = JsonGetBool(obj, "temporary");
	invite.uses      = JsonGetInt(obj, "uses");

	client->onevent(client, &invite);
}

static void Discord_EventHandlerInviteDelete(Discord::Client *client, const Json &data) {
	Discord::InviteDeleteEvent invite;
	Json_Object obj   = JsonGetObject(data);
	invite.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	invite.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	invite.code       = JsonGetString(obj, "code");
	client->onevent(client, &invite);
}

static void Discord_EventHandlerMessageCreate(Discord::Client *client, const Json &data) {
	Discord::MessageCreateEvent message;
	Discord_Deserialize(JsonGetObject(data), &message.message);
	client->onevent(client, &message);
}

static void Discord_EventHandlerMessageUpdate(Discord::Client *client, const Json &data) {
	Discord::MessageUpdateEvent message;
	Discord_Deserialize(JsonGetObject(data), &message.message);
	client->onevent(client, &message);
}

static void Discord_EventHandlerMessageDelete(Discord::Client *client, const Json &data) {
	Discord::MessageDeleteEvent message;
	Json_Object obj    = JsonGetObject(data);
	message.id         = Discord_ParseId(JsonGetString(obj, "id"));
	message.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	message.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent(client, &message);
}

static void Discord_EventHandlerMessageDeleteBulk(Discord::Client *client, const Json &data) {
	Discord::MessageDeleteBulkEvent message;
	Json_Object obj    = JsonGetObject(data);

	Json_Array ids = JsonGetArray(obj, "ids");
	message.ids.Resize(ids.count);
	for (ptrdiff_t index = 0; index < message.ids.count; ++index) {
		message.ids[index] = Discord_ParseId(JsonGetString(ids[index]));
	}
	
	message.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	message.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent(client, &message);
}

static void Discord_EventHandlerMessageReactionAdd(Discord::Client *client, const Json &data) {
	Discord::MessageReactionAddEvent reaction;
	Json_Object obj     = JsonGetObject(data);
	reaction.user_id    = Discord_ParseId(JsonGetString(obj, "user_id"));
	reaction.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	reaction.message_id = Discord_ParseId(JsonGetString(obj, "message_id"));
	reaction.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));

	const Json *member = obj.Find("member");
	if (member) {
		reaction.member = new Discord::GuildMember;
		if (reaction.member) {
			Discord_Deserialize(JsonGetObject(*member), reaction.member);
		}
	}

	Discord_Deserialize(JsonGetObject(obj, "emoji"), &reaction.emoji);
	client->onevent(client, &reaction);
}

static void Discord_EventHandlerMessageReactionRemove(Discord::Client *client, const Json &data) {
	Discord::MessageReactionRemoveEvent reaction;
	Json_Object obj     = JsonGetObject(data);
	reaction.user_id    = Discord_ParseId(JsonGetString(obj, "user_id"));
	reaction.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	reaction.message_id = Discord_ParseId(JsonGetString(obj, "message_id"));
	reaction.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(JsonGetObject(obj, "emoji"), &reaction.emoji);
	client->onevent(client, &reaction);
}

static void Discord_EventHandlerMessageReactionRemoveAll(Discord::Client *client, const Json &data) {
	Discord::MessageReactionRemoveAllEvent reaction;
	Json_Object obj     = JsonGetObject(data);
	reaction.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	reaction.message_id = Discord_ParseId(JsonGetString(obj, "message_id"));
	reaction.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent(client, &reaction);
}

static void Discord_EventHandlerMessageReactionRemoveEmoji(Discord::Client *client, const Json &data) {
	Discord::MessageReactionRemoveEmojiEvent reaction;
	Json_Object obj     = JsonGetObject(data);
	reaction.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	reaction.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	reaction.message_id = Discord_ParseId(JsonGetString(obj, "message_id"));
	Discord_Deserialize(JsonGetObject(obj, "emoji"), &reaction.emoji);
	client->onevent(client, &reaction);
}

static void Discord_EventHandlerPresenceUpdate(Discord::Client *client, const Json &data) {
	Discord::PresenceUpdateEvent presence;
	Discord_Deserialize(JsonGetObject(data), &presence.presence);
	client->onevent(client, &presence);
}

static constexpr Discord_Event_Handler DiscordEventHandlers[] = {
	Discord_EventHandlerNone, Discord_EventHandlerHello, Discord_EventHandlerReady,
	Discord_EventHandlerResumed, Discord_EventHandlerReconnect, Discord_EventHandlerInvalidSession,
	Discord_EventHandlerApplicationCommandPermissionsUpdate, Discord_EventHandlerChannelCreate,
	Discord_EventHandlerChannelUpdate, Discord_EventHandlerChannelDelete, Discord_EventHandlerChannelPinsUpdate,
	Discord_EventHandlerThreadCreate, Discord_EventHandlerThreadUpdate, Discord_EventHandlerThreadDelete,
	Discord_EventHandlerThreadListSync, Discord_EventHandlerThreadMemberUpdate, Discord_EventHandlerThreadMembersUpdate,
	Discord_EventHandlerGuildCreate, Discord_EventHandlerGuildUpdate, Discord_EventHandlerGuildDelete,
	Discord_EventHandlerGuildBanAdd, Discord_EventHandlerGuildBanRemove, Discord_EventHandlerGuildEmojisUpdate,
	Discord_EventHandlerGuildStickersUpdate, Discord_EventHandlerGuildIntegrationsUpdate, Discord_EventHandlerGuildMemberAdd,
	Discord_EventHandlerGuildMemberRemove, Discord_EventHandlerGuildMemberUpdate, Discord_EventHandlerGuildMembersChunk,
	Discord_EventHandlerGuildRoleCreate, Discord_EventHandlerGuildRoleUpdate, Discord_EventHandlerGuildRoleDelete,
	Discord_EventHandlerGuildScheduledEventCreate, Discord_EventHandlerGuildScheduledEventUpdate, Discord_EventHandlerGuildScheduledEventDelete,
	Discord_EventHandlerGuildScheduledEventUserAdd, Discord_EventHandlerGuildScheduledEventUserRemove,
	Discord_EventHandlerIntegrationCreate, Discord_EventHandlerIntegrationUpdate, Discord_EventHandlerIntegrationDelete,
	Discord_EventHandlerInteractionCreate,Discord_EventHandlerInviteCreate, Discord_EventHandlerInviteDelete,
	Discord_EventHandlerMessageCreate, Discord_EventHandlerMessageUpdate, Discord_EventHandlerMessageDelete,
	Discord_EventHandlerMessageDeleteBulk, Discord_EventHandlerMessageReactionAdd, Discord_EventHandlerMessageReactionRemove,
	Discord_EventHandlerMessageReactionRemoveAll, Discord_EventHandlerMessageReactionRemoveEmoji, Discord_EventHandlerPresenceUpdate,

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

	if (event->type == Discord::EventType::GUILD_CREATE) {
		auto guild = (Discord::GuildCreateEvent *)event;
		Trace("Joined guild: " StrFmt, StrArg(guild->guild.name));
		return;
	}

	if (event->type == Discord::EventType::GUILD_UPDATE) {
		auto guild = (Discord::GuildUpdateEvent *)event;
		Trace("Updated guild: " StrFmt, StrArg(guild->guild.name));
		return;
	}

	if (event->type == Discord::EventType::GUILD_DELETE) {
		auto guild = (Discord::GuildDeleteEvent *)event;
		if (guild->unavailable_guild.unavailable)
			Trace("Guild deleted: %zu", guild->unavailable_guild.id);
		else
			Trace("Removed from Guild: %zu", guild->unavailable_guild.id);
		return;
	}

	if (event->type == Discord::EventType::GUILD_BAN_ADD) {
		auto ban = (Discord::GuildBanAddEvent *)event;
		Trace("User banned: " StrFmt, StrArg(ban->user.username));
		return;
	}

	if (event->type == Discord::EventType::GUILD_BAN_REMOVE) {
		auto ban = (Discord::GuildBanAddEvent *)event;
		Trace("User unbanned: " StrFmt, StrArg(ban->user.username));
		return;
	}

	if (event->type == Discord::EventType::GUILD_EMOJIS_UPDATE) {
		auto emojis = (Discord::GuildEmojisUpdateEvent *)event;
		if (emojis->emojis.count)
			Trace("Emoji updated: " StrFmt, StrArg(emojis->emojis[0].name));
		else
			Trace("Emoji updated");
		return;
	}

	if (event->type == Discord::EventType::GUILD_STICKERS_UPDATE) {
		auto stickers = (Discord::GuildStickersUpdateEvent *)event;
		if (stickers->stickers.count)
			Trace("Sticker updated: " StrFmt, StrArg(stickers->stickers[0].name));
		else
			Trace("Sticker updated");
		return;
	}

	if (event->type == Discord::EventType::GUILD_INTEGRATIONS_UPDATE) {
		Trace("Guild integration update");
		return;
	}

	if (event->type == Discord::EventType::GUILD_MEMBER_ADD) {
		auto member = (Discord::GuildMemberAddEvent *)event;
		Trace("Member joined: " StrFmt, StrArg(member->member.user->username));
		return;
	}

	if (event->type == Discord::EventType::GUILD_MEMBER_REMOVE) {
		auto member = (Discord::GuildMemberRemoveEvent *)event;
		Trace("Member left: " StrFmt, StrArg(member->user.username));
		return;
	}

	if (event->type == Discord::EventType::GUILD_MEMBER_UPDATE) {
		auto member = (Discord::GuildMemberUpdateEvent *)event;
		Trace("Member updated: " StrFmt, StrArg(member->user.username));
		return;
	}

	if (event->type == Discord::EventType::GUILD_ROLE_CREATE) {
		auto role = (Discord::GuildRoleCreateEvent *)event;
		Trace("Role created: " StrFmt, StrArg(role->role.name));
		return;
	}
	
	if (event->type == Discord::EventType::GUILD_ROLE_UPDATE) {
		auto role = (Discord::GuildRoleUpdateEvent *)event;
		Trace("Role updated: " StrFmt, StrArg(role->role.name));
		return;
	}
	
	if (event->type == Discord::EventType::GUILD_ROLE_DELETE) {
		auto role = (Discord::GuildRoleDeleteEvent *)event;
		Trace("Role deleted: %zu", role->role_id.value);
		return;
	}

	if (event->type == Discord::EventType::GUILD_SCHEDULED_EVENT_CREATE) {
		auto scheduled_event = (Discord::GuildScheduledEventCreateEvent *)event;
		Trace("Scheduled event created: " StrFmt, StrArg(scheduled_event->scheduled_event.name));
		return;
	}

	if (event->type == Discord::EventType::GUILD_SCHEDULED_EVENT_UPDATE) {
		auto scheduled_event = (Discord::GuildScheduledEventUpdateEvent *)event;
		Trace("Scheduled event updated: " StrFmt, StrArg(scheduled_event->scheduled_event.name));
		return;
	}
	
	if (event->type == Discord::EventType::GUILD_SCHEDULED_EVENT_DELETE) {
		auto scheduled_event = (Discord::GuildScheduledEventDeleteEvent *)event;
		Trace("Scheduled event deleted: " StrFmt, StrArg(scheduled_event->scheduled_event.name));
		return;
	}
	
	if (event->type == Discord::EventType::GUILD_SCHEDULED_EVENT_USER_ADD) {
		auto scheduled_event = (Discord::GuildScheduledEventUserAddEvent *)event;
		Trace("Scheduled event user added: %zu ", scheduled_event->user_id.value);
		return;
	}
	
	if (event->type == Discord::EventType::GUILD_SCHEDULED_EVENT_USER_REMOVE) {
		auto scheduled_event = (Discord::GuildScheduledEventUserRemoveEvent *)event;
		Trace("Scheduled event user removed: %zu ", scheduled_event->user_id.value);
		return;
	}

	if (event->type == Discord::EventType::INTEGRATION_CREATE) {
		auto integration = (Discord::IntegrationCreateEvent *)event;
		Trace("Integration created: " StrFmt, StrArg(integration->integration.name));
		return;
	}

	if (event->type == Discord::EventType::INTEGRATION_UPDATE) {
		auto integration = (Discord::IntegrationUpdateEvent *)event;
		Trace("Integration updated: " StrFmt, StrArg(integration->integration.name));
		return;
	}
	
	if (event->type == Discord::EventType::INTEGRATION_DELETE) {
		auto integration = (Discord::IntegrationDeleteEvent *)event;
		Trace("Integration updated: %zu", integration->id.value);
		return;
	}

	if (event->type == Discord::EventType::INTERACTION_CREATE) {
		auto interaction = (Discord::InteractionCreateEvent *)event;
		Trace("Interaction created: %zu", interaction->interation.id);
		return;
	}

	if (event->type == Discord::EventType::INVITE_CREATE) {
		auto invite = (Discord::InviteCreateEvent *)event;
		Trace("Invite created: " StrFmt, StrArg(invite->code));
		return;
	}

	if (event->type == Discord::EventType::INVITE_DELETE) {
		auto invite = (Discord::InviteDeleteEvent *)event;
		Trace("Invite deleted: " StrFmt, StrArg(invite->code));
		return;
	}

	if (event->type == Discord::EventType::MESSAGE_CREATE) {
		auto msg = (Discord::MessageCreateEvent *)event;
		Trace("Message sent: \"" StrFmt "\" by " StrFmt, StrArg(msg->message.content), StrArg(msg->message.author.username));
		return;
	}
	
	if (event->type == Discord::EventType::MESSAGE_UPDATE) {
		auto msg = (Discord::MessageUpdateEvent *)event;
		Trace("Message updated: \"" StrFmt "\" by " StrFmt, StrArg(msg->message.content), StrArg(msg->message.author.username));
		return;
	}
	
	if (event->type == Discord::EventType::MESSAGE_DELETE) {
		auto msg = (Discord::MessageDeleteEvent *)event;
		Trace("Message deleted: %zu", msg->id.value);
		return;
	}
	
	if (event->type == Discord::EventType::MESSAGE_DELETE_BULK) {
		auto msg = (Discord::MessageDeleteBulkEvent *)event;
		Trace("Message bulk deleted from channel: %zu", msg->channel_id.value);
		return;
	}

	if (event->type == Discord::EventType::MESSAGE_REACTION_ADD) {
		auto reaction = (Discord::MessageReactionAddEvent *)event;
		Trace("Reaction added: " StrFmt, StrArg(reaction->emoji.name));
		return;
	}
	
	if (event->type == Discord::EventType::MESSAGE_REACTION_REMOVE) {
		auto reaction = (Discord::MessageReactionRemoveEvent *)event;
		Trace("Reaction removed: " StrFmt, StrArg(reaction->emoji.name));
		return;
	}

	if (event->type == Discord::EventType::MESSAGE_REACTION_REMOVE_ALL) {
		auto reaction = (Discord::MessageReactionRemoveAllEvent *)event;
		Trace("All reactions removed: %zu", reaction->message_id);
		return;
	}

	if (event->type == Discord::EventType::MESSAGE_REACTION_REMOVE_EMOJI) {
		auto reaction = (Discord::MessageReactionRemoveEmojiEvent *)event;
		Trace("All emoji reaction removed: " StrFmt, StrArg(reaction->emoji.name));
		return;
	}

	if (event->type == Discord::EventType::PRESENCE_UPDATE) {
		auto presence = (Discord::PresenceUpdateEvent *)event;
		Trace("Presence updated: %d", presence->presence.status);
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
	spec.queue_size = 64;
	spec.read_size  = MegaBytes(2);
	spec.write_size = KiloBytes(8);

	Websocket *websocket = Websocket_Connect(url, &res, &headers, spec);
	if (!websocket) {
		return 1;
	}

	MemoryArenaReset(client.scratch);

	int intents = 0;
	intents |= Discord::Intent::GUILDS;
	intents |= Discord::Intent::GUILD_BANS;
	intents |= Discord::Intent::GUILD_MEMBERS;
	intents |= Discord::Intent::GUILD_MESSAGES;
	intents |= Discord::Intent::GUILD_MESSAGE_REACTIONS;
	intents |= Discord::Intent::GUILD_EMOJIS_AND_STICKERS;
	intents |= Discord::Intent::GUILD_INTEGRATIONS;
	intents |= Discord::Intent::GUILD_SCHEDULED_EVENTS;
	intents |= Discord::Intent::GUILD_INVITES;
	intents |= Discord::Intent::GUILD_PRESENCES;
	intents |= Discord::Intent::DIRECT_MESSAGES;
	intents |= Discord::Intent::DIRECT_MESSAGE_REACTIONS;

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
