#pragma once
#include "Kr/KrBasic.h"
#include "Json.h"

namespace Discord {
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

	enum class VideoQualityMode {
		NONE = 0, AUTO = 1, FULL = 2
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
		VideoQualityMode video_quality_mode = VideoQualityMode::NONE;
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
		TICK,
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
		"",
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
		EventType type = EventType::TICK;
		String    name = "TICK";

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

	struct StageInstanceCreateEvent : public Event {
		StageInstance stage;
		StageInstanceCreateEvent(): Event(EventType::STAGE_INSTANCE_CREATE) {}
	};

	struct StageInstanceDeleteEvent : public Event {
		StageInstance stage;
		StageInstanceDeleteEvent(): Event(EventType::STAGE_INSTANCE_DELETE) {}
	};
	
	struct StageInstanceUpdateEvent : public Event {
		StageInstance stage;
		StageInstanceUpdateEvent(): Event(EventType::STAGE_INSTANCE_UPDATE) {}
	};

	struct TypingStartEvent : public Event {
		Snowflake    channel_id;
		Snowflake    guild_id;
		Snowflake    user_id;
		Timestamp    timestamp;
		GuildMember *member = nullptr;

		TypingStartEvent(): Event(EventType::TYPING_START) {}
	};

	struct UserUpdateEvent : public Event {
		User user;
		UserUpdateEvent(): Event(EventType::USER_UPDATE) {}
	};

	struct VoiceStateUpdateEvent : public Event {
		VoiceState voice_state;
		VoiceStateUpdateEvent(): Event(EventType::VOICE_STATE_UPDATE) {}
	};

	struct VoiceServerUpdateEvent : public Event {
		String    token;
		Snowflake guild_id;
		String    endpoint;
		VoiceServerUpdateEvent(): Event(EventType::VOICE_SERVER_UPDATE) {}
	};

	struct WebhooksUpdateEvent : public Event {
		Snowflake guild_id;
		Snowflake channel_id;
		WebhooksUpdateEvent(): Event(EventType::WEBHOOKS_UPDATE) {}
	};

	//
	//
	//

	using EventHandler = void(*)(struct Client *client, const struct Event *event);

	struct Heartbeat {
		float interval     = 2000.0f;
		float remaining    = 0.0f;
		int   count        = 0;
		int   acknowledged = 0;
	};

	struct Client;

	void IdentifyCommand(Client *client);
	void ResumeCommand(Client *client);
	void HearbeatCommand(Client *client);
	void GuildMembersRequestCommand(Client *client, const GuildMembersRequest &req_guild_mems);
	void VoiceStateUpdateCommand(Client *client, const VoiceStateUpdate &update_voice_state);
	void PresenceUpdateCommand(Client *client, const PresenceUpdate &presence_update);

	struct ClientSpec {
		int32_t          shards[2]    = { 0, 1 };
		int32_t          tick_ms      = 500;
		uint32_t         scratch_size = MegaBytes(512);
		uint32_t         read_size    = MegaBytes(2);
		uint32_t         write_size   = KiloBytes(8);
		uint32_t         queue_size   = 32;
		Memory_Allocator allocator    = ThreadContextDefaultParams.allocator;
	};

	struct Shard {
		int32_t id;
		int32_t count;
	};

	struct ShardSpec {
		Array_View<ClientSpec> specs;
		ClientSpec             default_spec;
	};

	void DefaultEventHandler(struct Client *client, const struct Event *event);

	void  Login(const String token, int32_t intents = 0, EventHandler onevent = DefaultEventHandler, PresenceUpdate *presence = nullptr, ClientSpec spec = ClientSpec());
	void  LoginSharded(const String token, int32_t intents = 0, EventHandler onevent = DefaultEventHandler, PresenceUpdate *presence = nullptr, int32_t shard_count = 0, const ShardSpec &specs = ShardSpec());
	void  Logout(Client *client);
	Shard GetShard(Client *client);

	void Initialize();

	//
	//
	//

	struct ChannelPatch {
		Json_Object obj;
	};

	void PatchName(ChannelPatch *p, String name);
	void PatchIcon(ChannelPatch *p, Buffer icon);
	void PatchChannelType(ChannelPatch *p, ChannelType type);
	void PatchPosition(ChannelPatch *p, int position);
	void PatchTopic(ChannelPatch *p, String topic);
	void PatchNSFW(ChannelPatch *p, bool value);
	void PatchRateLimitPerUser(ChannelPatch *p, int rate_limit_per_user);
	void PatchBitrate(ChannelPatch *p, int bitrate);
	void PatchUserLimit(ChannelPatch *p, int user_limit);
	void PatchOverwrites(ChannelPatch *p, Array_View<Overwrite> overwrites);
	void PatchParentId(ChannelPatch *p, Snowflake parent_id);
	void PatchRTCRegion(ChannelPatch *p, String rtc_region);
	void PatchVideoQualityMode(ChannelPatch *p, VideoQualityMode mode);
	void PatchDefaultAutoArchiveDuration(ChannelPatch *p, int duration);
	void PatchArchived(ChannelPatch *p, bool archived);
	void PatchAutoArchiveDuration(ChannelPatch *p, int auto_archive_duration);
	void PatchLocked(ChannelPatch *p, bool locked);
	void PatchInvitable(ChannelPatch *p, bool invitable);

	Channel *GetChannel(Client *client, Snowflake channel_id);
	Channel *ModifyChannel(Client *client, Snowflake channel_id, const ChannelPatch &patch);
	Channel *DeleteChannel(Client *client, Snowflake channel_id);
}
