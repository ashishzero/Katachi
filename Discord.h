#include "Basic.h"

namespace Discord {
	struct Snowflake {
		uint64_t value;
	};

	//
	// Permissions
	//

	enum class PermissionFlag : uint64_t {
		CREATE_INSTANT_INVITE = 1 << 0,
		KICK_MEMBERS = 1 << 1,
		BAN_MEMBERS  = 1 << 2,
		ADMINISTRATOR = 1 << 3,
		MANAGE_CHANNELS = 1 << 4,
		MANAGE_GUILD = 1 << 5,
		ADD_REACTIONS = 1 << 6,
		VIEW_AUDIT_LOG = 1 << 7,
		PRIORITY_SPEAKER = 1 << 8,
		STREAM = 1 << 9,
		VIEW_CHANNEL = 1 << 10,
		SEND_MESSAGES = 1 << 11,
		SEND_TTS_MESSAGES = 1 << 12,
		MANAGE_MESSAGES = 1 << 13,
		EMBED_LINKS = 1 << 14,
		ATTACH_FILES = 1 << 15,
		READ_MESSAGE_HISTORY = 1 << 16,
		MENTION_EVERYONE = 1 << 17,
		USE_EXTERNAL_EMOJIS = 1 << 18,
		VIEW_GUILD_INSIGHTS = 1 << 19,
		CONNECT = 1 << 20,
		SPEAK = 1 << 21,
		MUTE_MEMBERS = 1 << 22,
		DEAFEN_MEMBERS = 1 << 23,
		MOVE_MEMBERS = 1 << 24,
		USE_VAD = 1 << 25,
		CHANGE_NICKNAME = 1 << 26,
		MANAGE_NICKNAMES = 1 << 27,
		MANAGE_ROLES = 1 << 28,
		MANAGE_WEBHOOKS = 1 << 29,
		MANAGE_EMOJIS_AND_STICKERS = 1 << 30,
		USE_APPLICATION_COMMANDS = 1 << 31,
		REQUEST_TO_SPEAK = (uint64_t)1 << 32,
		MANAGE_EVENTS = (uint64_t)1 << 33,
		MANAGE_THREADS = (uint64_t)1 << 34,
		CREATE_PUBLIC_THREADS = (uint64_t)1 << 35,
		CREATE_PRIVATE_THREADS = (uint64_t)1 << 36,
		USE_EXTERNAL_STICKERS = (uint64_t)1 << 37,
		SEND_MESSAGES_IN_THREADS = (uint64_t)1 << 38,
		START_EMBEDDED_ACTIVITIES = (uint64_t)1 << 39,
		MODERATE_MEMBERS = (uint64_t)1 << 40
	};

	struct RoleTag {
		Snowflake bot_id;
		Snowflake integration_id;
		bool premium_subscriber;
	};

	struct Role {
		Snowflake id;
		String name;
		int32_t color;
		bool hoist;
		String icon;
		String unicode_emoji;
		int32_t position;
		String permissions;
		bool managed;
		bool mentionable;
		RoleTag *tags;
	};

	//
	// Integration
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
		String name;
		String icon;
		String description;
		String summary;
		struct User *bot;
	};

	struct Integration {
		Snowflake id;
		String name;
		String type;
		bool enabled;
		bool syncing;
		Snowflake role_id;
		bool enable_emoticons;
		IntegrationExpireBehavior expire_behavior;
		int32_t expire_grace_period;
		struct User *user;
		IntegrationAccount *account;
		uint64_t synced_at;
		int32_t subscriber_count;
		bool revoked;
		IntegrationApplication *application;
	};

	//
	// Users Resource
	//

	enum class Premium {
		NONE = 0,
		NITRO_CLASSIC = 1,
		NITRO = 2
	};

	enum class UserFlag {
		NONE = 0,
		STAFF = 1 << 0,
		PARTNER = 1 << 1,
		HYPESQUAD = 1 << 2,
		BUG_HUNTER_LEVEL_1 = 1 << 3,
		HYPESQUAD_ONLINE_HOUSE_1 = 1 << 6,
		HYPESQUAD_ONLINE_HOUSE_2 = 1 << 7,
		HYPESQUAD_ONLINE_HOUSE_3 = 1 << 8,
		PREMIUM_EARLY_SUPPORTER = 1 << 9,
		TEAM_PSEUDO_USER = 1 << 10,
		BUG_HUNTER_LEVEL_2 = 1 << 14,
		VERIFIED_BOT = 1 << 16,
		VERIFIED_DEVELOPER = 1 << 17,
		CERTIFIED_MODERATOR = 1 << 18,
		BOT_HTTP_INTERACTIONS = 1 << 19,
	};

	enum class VisibilityType {
		NONE = 0, EVERYONE = 1
	};

	struct Connection {
		String id;
		String name;
		String type;
		bool revoked;
		Array<Integration> integrations;
		bool verified;
		bool friend_sync;
		bool show_activity;
		VisibilityType visibility;
	};

	struct User {
		Snowflake id;
		String username;
		String discriminator;
		String avatar;
		bool bot;
		bool system;
		bool mfa_enabled;
		String banner;
		int32_t accent_color;
		String locale;
		bool verified;
		String email;
		int32_t flags;
		int32_t premium_type;
		int32_t public_flags;
	};

	//
	// Sticker Resource
	//

	enum class StickerType {
		STANDARD = 1, GUILD = 2
	};

	enum class StickerFormatType {
		PNG = 1, APNG = 2, LOTTIE = 3
	};

	struct StickerItem {
		Snowflake id;
		String name;
		StickerFormatType format_type;
	};

	struct Sticker {
		Snowflake id;
		Snowflake pack_id;
		String name;
		String description;
		String tags;
		String asset;
		int32_t type;
		int32_t format_type;
		bool available;
		Snowflake guild_id;
		User *user;
		int32_t sort_value;
	};

	struct StickerPack {
		Snowflake id;
		Array<Sticker> stickers;
		String name;
		Snowflake sku_id;
		Snowflake cover_sticker_id;
		String description;
		Snowflake banner_asset_id;
	};

	//
	// Stage Instance Resource
	//

	enum class PrivacyLevel {
		PUBLIC = 1, GUILD_ONLY = 2
	};

	struct StageInstance {
		Snowflake id;
		Snowflake guild_id;
		Snowflake channel_id;
		String topic;
		int32_t privacy_level;
		bool discoverable_disabled;
	};

	//
	// Voice Resource
	//

	struct VoiceState {
		Snowflake guild_id;
		Snowflake channel_id;
		Snowflake user_id;
		struct GuildMember *member;
		String session_id;
		bool deaf;
		bool mute;
		bool self_deaf;
		bool self_mute;
		bool self_stream;
		bool self_video;
		bool suppress;
		uint64_t request_to_speak_timestamp;
	};

	struct VoiceRegion {
		String id;
		String name;
		bool optimal;
		bool deprecated;
		bool custom;
	};

	//
	// Guild Scheduled Event
	//

	enum class GuildScheduledEventPrivacyLevel {
		GUILD_ONLY = 2
	};

	enum class GuildScheduledEventEntityType {
		STAGE_INSTANCE = 1, VOICE = 2, EXTERNAL = 3
	};

	enum class GuildScheduledEventStatus {
		SCHEDULED = 1, ACTIVE = 2, COMPLETED = 3, CANCELED = 4
	};

	struct GuildScheduledEventEntityMetadata {
		String location;
	};

	struct GuildScheduledEventUser {
		Snowflake guild_scheduled_event_id;
		User user;
		struct GuildMember *member;
	};

	struct GuildScheduledEvent {
		Snowflake id;
		Snowflake guild_id;
		Snowflake channel_id;
		Snowflake creator_id;
		String name;
		String description;
		uint64_t scheduled_start_time;
		uint64_t scheduled_end_time ;
		GuildScheduledEventPrivacyLevel privacy_level;
		GuildScheduledEventStatus status;
		GuildScheduledEventEntityType entity_type;
		Snowflake entity_id;
		GuildScheduledEventEntityMetadata entity_metadata;
		GuildScheduledEventUser *creator;
		int32_t user_count;
	};

	//
	// Emoji
	//

	struct Emoji {
		Snowflake id;
		String name;
		Array<Role> roles;
		User *user;
		bool require_colons;
		bool managed;
		bool animated;
		bool available;
	};

	//
// Channel
//

	enum class ChannelType {
		GUILD_TEXT = 0,
		DM = 1,
		GUILD_VOICE = 2,
		GROUP_DM = 3,
		GUILD_CATEGORY = 4,
		GUILD_NEWS = 5,
		GUILD_STORE = 6,
		GUILD_NEWS_THREAD = 10,
		GUILD_PUBLIC_THREAD = 11,
		GUILD_PRIVATE_THREAD = 12,
		GUILD_STAGE_VOICE = 13,
	};

	enum class VideoQualityMode {
		AUTO = 1, FULL = 2
	};

	struct ThreadMetadata {
		bool archived;
		int32_t auto_archive_duration;
		uint64_t archive_timestamp;
		bool locked;
		bool invitable;
		uint64_t create_timestamp;
	};

	struct ThreadMember {
		Snowflake id;
		Snowflake user_id;
		uint64_t join_timestamp;
		int32_t flags;
	};

	struct Overwrite {
		Snowflake id;
		int32_t type;
		String allow;
		String deny;
	};

	struct Channel {
		Snowflake id;
		int32_t type;
		Snowflake guild_id;
		int32_t position;
		Array<Overwrite> permission_overwrites;
		String name;
		String topic;
		bool nsfw;
		Snowflake last_message_id;
		int32_t bitrate;
		int32_t user_limit;
		int32_t rate_limit_per_user;
		Array<User> recipients;
		String icon;
		Snowflake owner_id;
		Snowflake application_id;
		Snowflake parent_id;
		uint64_t last_pin_timestamp;
		String rtc_region;
		int32_t video_quality_mode;
		int32_t message_count;
		int32_t member_count;
		ThreadMetadata thread_metadata;
		ThreadMember member;
		int32_t default_auto_archive_duration;
		String permissions;
	};

	//
	// Activity
	//

	enum class ActivityType {
		GAME, STREAMING, LISTENING, WATCHING, CUSTOM, COMPETING
	};

	enum class ActivityFlag {
		INSTANCE = 1 << 0,
		JOIN = 1 << 1,
		SPECTATE = 1 << 2,
		JOIN_REQUEST = 1 << 3,
		SYNC = 1 << 4,
		PLAY = 1 << 5,
		PARTY_PRIVACY_FRIENDS = 1 << 6,
		PARTY_PRIVACY_VOICE_CHANNEL = 1 << 7,
		EMBEDDED = 1 << 8
	};

	struct ActivityTimestamps {
		int32_t start;
		int32_t end;
	};

	struct ActivityEmoji {
		String name;
		Snowflake id;
		bool animated;
	};

	struct ActivityParty {
		String id;
		int32_t size[2];
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
		String name;
		ActivityType type;
		String url;
		int32_t created_at;
		ActivityTimestamps timestamps;
		Snowflake application_id;
		String details;
		String state;
		ActivityEmoji *emoji;
		ActivityParty *party;
		ActivityAssets *assets;
		ActivitySecrets *secrets;
		bool instance;
		uint32_t flags;
		Array<ActivityButton> buttons;
	};

	//
	// Guild
	//

	enum class MessageNotificationLevel {
		ALL_MESSAGES = 0, ONLY_MENTIONS = 1
	};

	enum class ExplicitContentFilerLevel {
		DISABLED = 0, MEMBERS_WITHOUT_ROLES = 1, ALL_MEMBERS = 2
	};

	enum class MFALevel {
		NONE = 0, ELEVATED = 1
	};

	enum class VerificationLevel {
		NONE = 0, LOW = 1, MEDIUM = 2, HIGH = 3, VERY_HIGH = 4
	};

	enum class GuildNSFWLevel {
		DEFAULT = 0, EXPLICIT = 1, SAFE = 2, AGE_RESTRICTED = 3
	};

	enum class PremiumTier {
		NONE = 0, TIER_1 = 1, TIER_2 = 2, TIER_3 = 3
	};

	enum class SystemChannelFlag {
		SUPPRESS_JOIN_NOTIFICATIONS = 1 << 0,
		SUPPRESS_PREMIUM_SUBSCRIPTIONS = 1 << 1,
		SUPPRESS_GUILD_REMINDER_NOTIFICATIONS = 1 << 2,
		SUPPRESS_JOIN_NOTIFICATION_REPLIES = 1 << 3
	};

	enum class GuildFeature {
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
		NEW,
		PARTNERED,
		PREVIEW_ENABLED,
		PRIVATE_THREADS,
		ROLE_ICONS,
		SEVEN_DAY_THREAD_ARCHIVE,
		THREE_DAY_THREAD_ARCHIVE,
		TICKETED_EVENTS_ENABLED,
		VANITY_URL,
		VERIFIED,
		VIP_REGIONS,
		WELCOME_SCREEN_ENABLED,
	};

	struct UnavailableGuild {
		Snowflake id;
		bool unavailable;
	};

	struct GuildPreview {
		Snowflake id;
		String name;
		String icon;
		String splash;
		String discovery_splash;
		Array<Emoji> emojis;
		Array<GuildFeature> features;
		int32_t approximate_member_count;
		int32_t approximate_presence_count;
		String description;
		Array<Sticker> stickers;
	};

	struct GuildWidgetSettings {
		bool enabled;
		Snowflake channel_id;
	};

	struct GuildWidget {
		Snowflake id;
		String name;
		String instant_invite;
		Array<Channel> channels;
		Array<User> members;
		int32_t presence_count;
	};

	struct GuildMember {
		User *user;
		String nick;
		String avatar;
		Array<Snowflake> roles;
		uint64_t joined_at;
		uint64_t premium_since;
		bool deaf;
		bool mute;
		bool pending;
		String permissions;
		uint64_t communication_disabled_until;
	};

	struct Ban {
		String reason;
		User *user;
	};

	struct WelcomeScreenChannel {
		Snowflake channel_id;
		String description;
		Snowflake emoji_id;
		String emoji_name;
	};

	struct WelcomeScreen {
		String description;
		Array<WelcomeScreenChannel> welcome_channels;
	};

	enum class StatusType {
		ONLINE, DO_NOT_DISTURB, AFK, INVISIBLE, OFFLINE
	};

	struct Presence {
		User user;
		Snowflake guild_id;
		StatusType status;
		Array<Activity> activities;
		struct {
			String desktop;
			String mobile;
			String web;
		} client_status;
	};

	struct Guild {
		Snowflake id;
		String name;
		String icon;
		String icon_hash;
		String splash;
		String discovery_splash;
		bool owner;
		Snowflake owner_id;
		String permissions;
		String region;
		Snowflake afk_channel_id;
		int32_t afk_timeout;
		bool widget_enabled;
		Snowflake widget_channel_id;
		int32_t verification_level;
		int32_t default_message_notifications;
		int32_t explicit_content_filter;
		Array<Role> roles;
		Array<Emoji> emojis;
		Array<GuildFeature> features;
		int32_t mfa_level;
		Snowflake application_id;
		Snowflake system_channel_id;
		int32_t system_channel_flags;
		Snowflake rules_channel_id;
		uint64_t joined_at;
		bool large;
		bool unavailable;
		int32_t member_count;
		Array<VoiceState> voice_states;
		Array<GuildMember> members;
		Array<Channel> channels;
		Array<Channel> threads;
		Array<Presence> presences;
		int32_t max_presences;
		int32_t max_members;
		String vanity_url_code;
		String description;
		String banner;
		int32_t premium_tier;
		int32_t premium_subscription_count;
		String preferred_locale;
		Snowflake public_updates_channel_id;
		int32_t max_video_channel_users;
		int32_t approximate_member_count;
		int32_t approximate_presence_count;
		WelcomeScreen *welcome_screen;
		int32_t nsfw_level;
		Array<StageInstance> stage_instances;
		Array<Sticker> stickers;
		Array<GuildScheduledEvent> guild_scheduled_events;
		bool premium_progress_bar_enabled;
	};

	//
	// Guild Template
	//

	struct GuildTemplate {
		String code;
		String name;
		String description;
		int32_t usage_count;
		Snowflake creator_id;
		User *creator;
		uint64_t created_at;
		uint64_t updated_at;
		Snowflake source_guild_id;
		Guild serialized_source_guild;
		bool is_dirty;
	};

	//
	// Message Components
	//

	enum class ComponentType {
		ACTION_ARROW = 1, BUTTON = 2, SELECT_MENU = 3
	};

	enum class ButtonStyle {
		PRIMARY = 1, SECONDARY = 2, SUCCESS = 3, DANGER = 4, LINK = 5
	};

	struct Button {
		int32_t type;
		ButtonStyle style;
		String label;
		Emoji *emoji;
		String custom_id;
		String url;
		bool disabled;
	};

	struct SelectOption {
		String label;
		String value;
		String description;
		Emoji *emoji;
		bool default_;
	};

	struct Menu {
		int32_t type;
		String custom_id;
		Array<SelectOption> options;
		String placeholder;
		int32_t min_values;
		int32_t max_values;
		bool disabled;
	};

	struct Component {
		ComponentType type;
		String custom_id;
		bool disabled;
		ButtonStyle style;
		String label;
		Emoji *emoji;
		String url;
		Array<SelectOption> options;
		String placeholder;
		int32_t min_values;
		int32_t max_values;
		Array<Component *> components;
	};

	//
	// Message
	//

	enum class MessageType {
		DEFAULT = 0,
		RECIPIENT_ADD = 1,
		RECIPIENT_REMOVE = 2,
		CALL = 3,
		CHANNEL_NAME_CHANGE = 4,
		CHANNEL_ICON_CHANGE = 5,
		CHANNEL_PINNED_MESSAGE = 6,
		GUILD_MEMBER_JOIN = 7,
		USER_PREMIUM_GUILD_SUBSCRIPTION = 8,
		USER_PREMIUM_GUILD_SUBSCRIPTION_TIER_1 = 9,
		USER_PREMIUM_GUILD_SUBSCRIPTION_TIER_2 = 10,
		USER_PREMIUM_GUILD_SUBSCRIPTION_TIER_3 = 11,
		CHANNEL_FOLLOW_ADD = 12,
		GUILD_DISCOVERY_DISQUALIFIED = 14,
		GUILD_DISCOVERY_REQUALIFIED = 15,
		GUILD_DISCOVERY_GRACE_PERIOD_INITIAL_WARNING = 16,
		GUILD_DISCOVERY_GRACE_PERIOD_FINAL_WARNING = 17,
		THREAD_CREATED = 18,
		REPLY = 19,
		CHAT_INPUT_COMMAND = 20,
		THREAD_STARTER_MESSAGE = 21,
		GUILD_INVITE_REMINDER = 22,
		CONTEXT_MENU_COMMAND = 23,
	};

	enum class MessageActivityType {
		JOIN = 1,
		SPECTATE = 2,
		LISTEN = 3,
		JOIN_REQUEST = 5,
	};

	enum class MessageFlag {
		CROSSPOSTED = 1 << 0,
		IS_CROSSPOST = 1 << 1,
		SUPPRESS_EMBEDS = 1 << 2,
		SOURCE_MESSAGE_DELETED = 1 << 3,
		URGENT = 1 << 4,
		HAS_THREAD = 1 << 5,
		EPHEMERAL = 1 << 6,
		LOADING = 1 << 7,
	};

	struct MessageActivity {
		int32_t type;
		String party_id;
	};

	struct MessageReference {
		Snowflake message_id;
		Snowflake channel_id;
		Snowflake guild_id;
		bool fail_if_not_exists;
	};

	struct FollowedChannel {
		Snowflake channel_id;
		Snowflake webhook_id;
	};

	struct Reaction {
		int32_t count;
		bool me;
		Emoji emoji;
	};

	#define EMBED_TYPE_RICH 		"rich"
	#define EMBED_TYPE_IMAGE 		"image"
	#define EMBED_TYPE_VIDEO 		"video"
	#define EMBED_TYPE_GIFV 		"gifv"
	#define EMBED_TYPE_ARTICLE 		"article"
	#define EMBED_TYPE_LINK 		"link"

	struct EmbedThumbnail {
		String url;
		String proxy_url;
		int32_t height;
		int32_t width;
	};

	struct EmbedVideo {
		String url;
		String proxy_url;
		int32_t height;
		int32_t width;
	};

	struct EmbedImage {
		String url;
		String proxy_url;
		int32_t height;
		int32_t width;
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

	struct EmbedFooter {
		String text;
		String icon_url;
		String proxy_icon_url;
	};

	struct EmbedField {
		String name;
		String value;
		bool inline_;
	};

	struct Embed {
		String title;
		String type;
		String description;
		String url;
		uint64_t timestamp;
		int32_t color;
		EmbedFooter *footer;
		EmbedImage *image;
		EmbedThumbnail *thumbnail;
		EmbedVideo *video;
		EmbedProvider *provider;
		EmbedAuthor *author;
		Array<EmbedField> fields;
	};

	struct Attachment {
		String id;
		String filename;
		String description;
		String content_type;
		int32_t size;
		String url;
		String proxy_url;
		int32_t height;
		int32_t width;
		bool ephemeral;
	};

	struct ChannelMention {
		Snowflake id;
		Snowflake guild_id;
		int32_t type;
		String name;
	};

	const String AllowedMentionRole = "roles";
	const String AllowedMentionUser = "users";
	const String AllowedMentionEveryone = "everyone";

	struct AllowedMention {
		Array<String> parse;
		Array<Snowflake> roles;
		Array<Snowflake> users;
		bool replied_user;
	};

	constexpr uint32_t EMBED_LIMIT_TITLE = 256;
	constexpr uint32_t EMBED_LIMIT_DESCRIPTION = 4096;
	constexpr uint32_t EMBED_LIMIT_FIELDS = 25;
	constexpr uint32_t EMBED_LIMIT_FIELD_NAME = 256;
	constexpr uint32_t EMBED_LIMIT_FIELD_VALUE = 1024;
	constexpr uint32_t EMBED_LIMIT_FIELD_TEXT = 2048;
	constexpr uint32_t EMBED_LIMIT_AUTHOR_NAME = 256;
	constexpr uint32_t EMBED_LIMIT_COMBINED = 6000;

	struct Message {
		Snowflake id;
		Snowflake channel_id;
		Snowflake guild_id;
		User author;
		GuildMember member;
		String content;
		uint64_t timestamp;
		uint64_t edited_timestamp;
		bool tts;
		bool mention_everyone;
		Array<User> mentions;
		Array<Role> mention_roles;
		Array<ChannelMention> mention_channels;
		Array<Attachment> attachments;
		Array<Embed> embeds;
		Array<Reaction> reactions;
		String nonce;
		bool pinned;
		Snowflake webhook_id;
		int32_t type;
		MessageActivity *activity;
		struct Application *application;
		Snowflake application_id;
		MessageReference *message_reference;
		int32_t flags;
		struct Message *referenced_message;
		struct MessageInteraction *interaction;
		Channel *thread;
		Array<Component> components;
		Array<StickerItem> sticker_items;
		Array<Sticker> stickers;
	};

	//
	// Application 
	//

	struct Application {
		Snowflake id;
		String name;
		String icon;
		String description;
		Array<String> rpc_origins;
		bool bot_public;
		bool bot_require_code_grant;
		String terms_of_service_url;
		String privacy_policy_url;
		User *owner;
		String summary;
		String verify_key;
		struct Team *team;
		Snowflake guild_id;
		Snowflake primary_sku_id;
		String slug;
		String cover_image;
		String flags;
	};

	enum class ApplicationFlag : uint32_t {
		NONE = 0,
		GATEWAY_PRESENCE = 1 << 12,
		GATEWAY_PRESENCE_LIMITED = 1 << 13,
		GATEWAY_GUILD_MEMBERS = 1 << 14,
		GATEWAY_GUILD_MEMBERS_LIMITED = 1 << 15,
		VERIFICATION_PENDING_GUILD_LIMIT = 1 << 16,
		EMBEDDED = 1 << 17,
		GATEWAY_MESSAGE_CONTENT = 1 << 18,
		GATEWAY_MESSAGE_CONTENT_LIMITED = 1 << 19,
	};


	//
	// Application Commands
	//

	enum class ApplicationCommandOptionType {
		SUB_COMMAND = 1,
		SUB_COMMAND_GROUP = 2,
		STRING = 3,
		INTEGER = 4,
		BOOLEAN = 5,
		USER = 6,
		CHANNEL = 7,
		ROLE = 8,
		MENTIONABLE = 9,
		NUMBER = 10,
	};

	struct ApplicationCommandOptionChoice {
		String name;
		union {
			uint8_t string[101];
			int32_t integer;
			double number;
		} value;
	};

	struct ApplicationCommandOption {
		ApplicationCommandOptionType type;
		String name;
		String description;
		bool required;
		Array<ApplicationCommandOptionChoice> choices;
		Array<ApplicationCommandOption *> options;
		Array<ChannelType> channel_types;
		union { int32_t integer; double number; } min_value;
		union { int32_t integer; double number; } max_value;
		bool autocomplete;
	};

	struct ApplicationCommandInteractionDataOption {
		String name;
		ApplicationCommandOptionType type;
		union {
			uint8_t string[101];
			int32_t integer;
			double number;
		} value;
		Array<ApplicationCommandInteractionDataOption *> options;
		bool focused;
	};

	enum class ApplicationCommandType {
		CHAT_INPUT = 1, USER = 2, MESSAGE = 3
	};

	struct ApplicationCommand {
		Snowflake id;
		ApplicationCommandType type;
		Snowflake application_id;
		Snowflake guild_id;
		String name;
		String description;
		Array<ApplicationCommandOption> options;
		bool default_permission;
		Snowflake version;
	};

	enum class ApplicationCommandPermissionType {
		ROLE = 1, USER = 2
	};

	struct ApplicationCommandPermission {
		Snowflake id;
		ApplicationCommandPermissionType type;
		bool permission;
	};

	struct ApplicationCommandPermissions {
		Snowflake id;
		Snowflake application_id;
		Snowflake guild_id;
		Array<ApplicationCommandPermission> permissions;
	};

	//
	// Interaction
	//

	enum class InteractionType {
		PING = 1,
		APPLICATION_COMMAND = 2,
		MESSAGE_COMPONENT = 3,
		APPLICATION_COMMAND_AUTOCOMPLETE = 4
	};

	struct InteractionData {
		template <typename T>
		struct Map {
			Snowflake id;
			T value;
		};

		struct ResolvedData {
			Array<Map<User>> users;
			Array<Map<GuildMember>> members;
			Array<Map<Role>> roles;
			Array<Map<Channel>> channels;
			Array<Map<Message>> messages;
		};

		Snowflake id;
		String name;
		ApplicationCommandType type;
		ResolvedData resolved;
		Array<ApplicationCommandInteractionDataOption> options;
		String custom_id;
		ComponentType component_type;
		Array<SelectOption> values;
		Snowflake target_id;
	};

	struct Interaction {
		Snowflake id;
		Snowflake application_id;
		InteractionType type;
		InteractionData *data;
		Snowflake guild_id;
		Snowflake channel_id;
		GuildMember *member;
		User *user;
		String token;
		int32_t version;
		Message *message;
		String locale;
		String guild_locale;
	};

	struct MessageInteraction {
		Snowflake id;
		InteractionType type;
		String name;
		User user;
		GuildMember *member;
	};

	enum class InteractionCallbackType {
		PONG = 1,
		CHANNEL_MESSAGE_WITH_SOURCE = 4,
		DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE = 5,
		DEFERRED_UPDATE_MESSAGE = 6,
		UPDATE_MESSAGE = 7,
		APPLICATION_COMMAND_AUTOCOMPLETE_RESULT = 8
	};

	struct InteractionCallbackData {
		bool tts;
		String content;
		Array<Embed> embeds;
		AllowedMention *allowed_mentions;
		MessageFlag flags;
		Array<Component> components;
		Array<Attachment> attachments;
	};

	struct InteractionResponse {
		InteractionCallbackType type;
		InteractionCallbackData data;
	};

	//
	// Invite Resource
	//

	enum class InviteTargetType {
		STREAM = 1, EMBEDDED_APPLICATION = 2
	};

	struct InviteMetadata {
		int32_t uses;
		int32_t max_uses;
		int32_t max_age;
		bool temporary;
		uint64_t created_at;
	};

	struct InviteStageInstance {
		Array<GuildMember> members;
		int32_t participant_count;
		int32_t speaker_count;
		String topic;
	};

	struct Invite {
		String code;
		Guild guild;
		Channel channel;
		User inviter;
		int32_t target_type;
		User *target_user;
		Application *target_application;
		int32_t approximate_presence_count;
		int32_t approximate_member_count;
		uint64_t expires_at;
		InviteStageInstance *stage_instance;
		GuildScheduledEvent *guild_scheduled_event;
	};

	//
	// Teams
	//

	enum class MembershipState {
		INVITED = 1, ACCEPTED = 2
	};

	struct TeamMember {
		MembershipState membership_state;
		Array<String> permissions;
		Snowflake team_id;
		User user;
	};

	struct Team {
		String icon;
		Snowflake id;
		Array<TeamMember> members;
		String name;
		Snowflake owner_user_id;
	};

	//
	// Webhook Resource
	//

	enum class WebhookType {
		INCOMING = 1, CHANNEL_FOLLOWER = 2, APPLICATION = 3
	};

	struct Webhook {
		Snowflake id;
		WebhookType type;
		Snowflake guild_id;
		Snowflake channel_id;
		User *user;
		String name;
		String avatar;
		String token;
		Snowflake application_id;
		Guild *source_guild;
		Channel *source_channel;
		String url;
	};

	//
	// Audits
	//

	enum class AuditLogEvent {
		GUILD_UPDATE = 1,
		CHANNEL_CREATE = 10,
		CHANNEL_UPDATE = 11,
		CHANNEL_DELETE = 12,
		CHANNEL_OVERWRITE_CREATE = 13,
		CHANNEL_OVERWRITE_UPDATE = 14,
		CHANNEL_OVERWRITE_DELETE = 15,
		MEMBER_KICK = 20,
		MEMBER_PRUNE = 21,
		MEMBER_BAN_ADD = 22,
		MEMBER_BAN_REMOVE = 23,
		MEMBER_UPDATE = 24,
		MEMBER_ROLE_UPDATE = 25,
		MEMBER_MOVE = 26,
		MEMBER_DISCONNECT = 27,
		BOT_ADD = 28,
		ROLE_CREATE = 30,
		ROLE_UPDATE = 31,
		ROLE_DELETE = 32,
		INVITE_CREATE = 40,
		INVITE_UPDATE = 41,
		INVITE_DELETE = 42,
		WEBHOOK_CREATE = 50,
		WEBHOOK_UPDATE = 51,
		WEBHOOK_DELETE = 52,
		EMOJI_CREATE = 60,
		EMOJI_UPDATE = 61,
		EMOJI_DELETE = 62,
		MESSAGE_DELETE = 72,
		MESSAGE_BULK_DELETE = 73,
		MESSAGE_PIN = 74,
		MESSAGE_UNPIN = 75,
		INTEGRATION_CREATE = 80,
		INTEGRATION_UPDATE = 81,
		INTEGRATION_DELETE = 82,
		STAGE_INSTANCE_CREATE = 83,
		STAGE_INSTANCE_UPDATE = 84,
		STAGE_INSTANCE_DELETE = 85,
		STICKER_CREATE = 90,
		STICKER_UPDATE = 91,
		STICKER_DELETE = 92,
		GUILD_SCHEDULED_EVENT_CREATE = 100,
		GUILD_SCHEDULED_EVENT_UPDATE = 101,
		GUILD_SCHEDULED_EVENT_DELETE = 102,
		THREAD_CREATE = 110,
		THREAD_UPDATE = 111,
		THREAD_DELETE = 112,
	};

	struct OptionalAudioEntryInfo {
		Snowflake channel_id;
		String count;
		String delete_member_days;
		Snowflake id;
		String members_removed;
		Snowflake message_id;
		String role_name;
		String type;
	};

	struct AuditLogChangeKey {
		Snowflake afk_channel_id;
		int32_t afk_timeout;
		String allow;
		Snowflake application_id;
		bool archived;
		String asset;
		int32_t auto_archive_duration;
		bool available;
		String avatar_hash;
		String banner_hash;
		int32_t bitrate;
		Snowflake channel_id;
		String code;
		int32_t color;
		uint64_t communication_disabled_until;
		bool deaf;
		int32_t default_auto_archive_duration;
		int32_t default_message_notifications;
		String deny;
		String description;
		String discovery_splash_hash;
		bool enable_emoticons;
		int32_t entity_type;
		int32_t expire_behavior;
		int32_t expire_grace_period;
		int32_t explicit_content_filter;
		StickerFormatType format_type;
		Snowflake guild_id;
		bool hoist;
		String icon_hash;
		Snowflake id;
		bool invitable;
		Snowflake inviter_id;
		String location;
		bool locked;
		int32_t max_age;
		int32_t max_uses;
		bool mentionable;
		int32_t mfa_level;
		bool mute;
		String name;
		String nick;
		bool nsfw;
		Snowflake owner_id;
		Array<Overwrite> permission_overwrites;
		String permissions;
		int32_t position;
		String preferred_locale;
		PrivacyLevel privacy_level;
		int32_t prune_delete_days;
		Snowflake public_updates_channel_id;
		int32_t rate_limit_per_user;
		String region;
		Snowflake rules_channel_id;
		String splash_hash;
		GuildScheduledEventStatus status;
		Snowflake system_channel_id;
		String tags;
		bool temporary;
		String topic;
		String type;
		String unicode_emoji;
		int32_t user_limit;
		int32_t uses;
		String vanity_url_code;
		int32_t verification_level;
		Snowflake widget_channel_id;
		bool widget_enabled;
		Array<Role> add;
		Array<Role> remove;
	};

	struct AuditLogChange {
		AuditLogChangeKey *new_value;
		AuditLogChangeKey *old_value;
		String key;
	};

	struct AuditLogEntry {
		String target_id;
		Array<AuditLogChange> changes;
		Snowflake user_id;
		Snowflake id;
		AuditLogEvent action_type;
		OptionalAudioEntryInfo *options;
		String reason;
	};

	struct AuditLog {
		Array<AuditLogEntry> audit_log_entries;
		Array<GuildScheduledEvent> guild_scheduled_events;
		Array<Integration> integrations;
		Array<Channel> channels;
		Array<User> users;
		Array<Webhook> webhooks;
	};

	//
	// Gateways
	//

	enum class GatewayIntent {
		GUILDS = 1 << 0,
		GUILD_MEMBERS = 1 << 1,
		GUILD_BANS = 1 << 2,
		GUILD_EMOJIS_AND_STICKERS = 1 << 3,
		GUILD_INTEGRATIONS = 1 << 4,
		GUILD_WEBHOOKS = 1 << 5,
		GUILD_INVITES = 1 << 6,
		GUILD_VOICE_STATES = 1 << 7,
		GUILD_PRESENCES = 1 << 8,
		GUILD_MESSAGES = 1 << 9,
		GUILD_MESSAGE_REACTIONS = 1 << 10,
		GUILD_MESSAGE_TYPING = 1 << 11,
		DIRECT_MESSAGES = 1 << 12,
		DIRECT_MESSAGE_REACTIONS = 1 << 13,
		DIRECT_MESSAGE_TYPING = 1 << 14,
		GUILD_SCHEDULED_EVENTS = 1 << 15,
	};

	enum class GatewayCommand {
		IDENTIFY, RESUME, HEARTBEAT, GUILD_MEMBERS_REQUEST, VOICE_STATE_UPDATE, PRESENCE_UPDATE
	};

	struct GatewayCommandPresenceUpdate {
		int32_t since;
		Array<Activity> activities;
		StatusType status;
		bool afk;
	};

	struct PresenceUpdate {
		int32_t since;
		Array<Activity> activities;
		StatusType status;
		bool afk;
	};

	struct GatewayCommandIdentify {
		String token;
		struct {
			String os;
			String browser;
			String device;
		} properties;
		bool compress;
		int32_t large_threshold;
		int32_t shard[2];
		PresenceUpdate *presence;
		uint32_t intents;
	};

	struct GatewayCommandResume {
		String token;
		String session_id;
		int32_t seq;
	};

	struct GatewayCommandHeartbeat {
		int32_t op;
		int32_t d;
	};

	struct GatewayCommandGuildMembersRequest {
		Snowflake guild_id;
		String query;
		int32_t limit;
		bool presences;
		Array<Snowflake> user_ids;
		String nonce;
	};

	struct GatewayCommandVoiceStateUpdate {
		Snowflake guild_id;
		Snowflake channel_id;
		bool self_mute;
		bool self_deaf;
	};

	enum class GatewayEvent {
		HELLO, READY, RESUMED, RECONNECT, INVALID_SESSION, 

		CHANNEL_CREATE, CHANNEL_UPDATE, CHANNEL_DELETE, CHANNEL_PINS_UPDATE,

		THREAD_CREATE, THREAD_DELETE, THREAD_LIST_SYNC, THREAD_MEMBER_UPDATE, THREAD_MEMBERS_UPDATE,

		GUILD_CREATE, GUILD_UPDATE, GUILD_DELETE, GUILD_BAN_ADD, BUILD_BAN_REMOVE, 
		GUILD_EMOJIS_UPDATE, GUILD_STICKERS_UPDATE, GUILD_INTEGRATIONS_UPDATE, GUILD_MEMBER_ADD,
		GUILD_MEMBER_REMOVE, GUILD_MEMBER_UPDATE, GUILD_MEMBERS_CHUNK, GUILD_ROLE_CREATE,
		GUILD_ROLE_UPDATE, GUILD_ROLE_DELETE,

		GUILD_SCHEDULED_EVENT_CREATE, GUILD_SCHEDULED_EVENT_UPDATE,
		GUILD_SCHEDULED_EVENT_DELETE, GUILD_SCHEDULED_EVENT_USER_ADD,
		GUILD_SCHEDULED_EVENT_USER_REMOVE,

		INTEGRATION_CREATE, INTEGRATION_UPDATE, INTEGRATION_DELETE, INTERACTION_CREATE,

		INVITE_CREATE, INVITE_DELETE,

		MESSAGE_CREATE, MESSAGE_UPDATE, MESSAGE_DELETE, MESSAGE_DELETE_BULK,
		MESSAGE_REACTION_ADD, MESSAGE_REACTION_REMOVE, MESSAGE_REACTION_REMOVE_ALL,
		MESSAGE_REACTION_REMOVE_EMOJI,

		PRESENCE_UPDATE,

		STAGE_INSTANCE_CREATE, STAGE_INSTANCE_DELETE, STAGE_INSTANCE_UPDATE,

		TYPING_START,
		USER_UPDATE,
		VOICE_STATE_UPDATE, VOICE_SERVER_UPDATE,
		WEBHOOKS_UPDATE
	};

	struct GatewayEventHello {
		int32_t heartbeat_interval;
	};

	struct GatewayEventReady {
		int32_t v;
		User *user;
		Array<UnavailableGuild> guilds;
		String session_id;
		int32_t shard[2];
		Application *application;
	};

	struct GatewayEventReconnect {
		int32_t op;
		bool d;
	};

	struct GatewayEventInvalidSession {
		int32_t op;
		bool d;
	};

	struct GatewayEventChannelCreate {
		Channel channel;
	};

	struct GatewayEventChannelUpdate {
		Channel channel;
	};

	struct GatewayEventChannelDelete {
		Channel channel;
	};

	struct GatewayEventChannelPinsUpdate {
		Snowflake guild_id;
		Snowflake channel_id;
		uint64_t last_pin_timestamp;
	};

	struct GatewayEventThreadCreate {
		Channel channel;
		ThreadMember *member;
	};

	struct GatewayEventThreadUpdate {
		Channel channel;
	};

	struct GatewayEventThreadDelete {
		Channel channel;
	};

	struct GatewayEventThreadListSync {
		Snowflake guild_id;
		Array<Snowflake> channel_ids;
		Array<Channel> threads;
		Array<ThreadMember> members;
	};

	struct GatewayEventThreadMemberUpdate {
		Snowflake guild_id;
	};

	struct GatewayEventThreadMembersUpdate {
		Snowflake id;
		Snowflake guild_id;
		int32_t member_count;
		Array<ThreadMember> added_members;
		Array<Snowflake> removed_member_ids;
	};

	struct GatewayEventPresenceUpdate {
		Presence presence;
	};

	struct GatewayEventGuildCreate {
		Guild guild;
	};

	struct GatewayEventGuildUpdate {
		Guild guild;
	};

	struct GatewayEventGuildDelete {
		UnavailableGuild guild;
	};

	struct GatewayEventGuildBanAdd {
		Snowflake guild_id;
		User user;
	};

	struct GatewayEventGuildBanRemove {
		Snowflake guild_id;
		User user;
	};

	struct GatewayGuildEmojiUpdate {
		Snowflake guild_id;
		Array<Emoji> emojis;
	};

	struct GatewayGuildStickerUpdate {
		Snowflake guild_id;
		Array<Sticker> stickers;
	};

	struct GatewayEventGuildIntegrationUpdate {
		Snowflake guild_id;
	};

	struct GatewayEventGuildMemberAdd {
		Snowflake guild_id;
		GuildMember member;
	};

	struct GatewayEventGuildMemberRemove {
		Snowflake guild_id;
		User user;
	};

	struct GatewayEventGuildMemberUpdate {
		Snowflake guild_id;
		Array<Snowflake> roles;
		User user;
		String nick;
		String avatar;
		uint64_t joined_at;
		uint64_t premium_since;
		bool deaf;
		bool mute;
		bool pending;
		uint64_t communication_disabled_until;
	};

	struct GatewayEventMemberChunk {
		Snowflake guild_id;
		Array<GuildMember> members;
		int32_t chunk_index;
		int32_t chunk_count;
		Array<Snowflake> not_found;
		Array<Presence> presences;
		String nonce;
	};

	struct GatewayEventRoleCreate {
		Snowflake guild_id;
		Role role;
	};

	struct GatewayEventRoleUpdate {
		Snowflake guild_id;
		Role role;
	};

	struct GatewayEventRoleDelete {
		Snowflake guild_id;
		Snowflake role_id;
	};

	struct GatewayEventGuildScheduledEventCreate {
		GuildScheduledEvent scheduled_event;
	};

	struct GatewayEventGuildScheduledEventUpdate {
		GuildScheduledEvent scheduled_event;
	};

	struct GatewayEventGuildScheduledEventDelete {
		GuildScheduledEvent scheduled_event;
	};

	struct GatewayEventGuildScheduledUserAdd {
		Snowflake guild_scheduled_event_id;
		Snowflake user_id;
		Snowflake guild_id;
	};

	struct GatewayEventGuildScheduledUserRemove {
		Snowflake guild_scheduled_event_id;
		Snowflake user_id;
		Snowflake guild_id;
	};

	struct GatewayEventIntegrationCreate {
		Snowflake guild_id;
		Integration integration;
	};

	struct GatewayEventIntegrationUpdate {
		Snowflake guild_id;
		Integration integration;
	};

	struct GatewayEventIntegrationDelete {
		Snowflake id;
		Snowflake guild_id;
		Snowflake application_id;
	};

	struct GatewayEventInterationCreate {
		Interaction interaction;
	};

	struct GatewayEventInviteCreate {
		Snowflake channel_id;
		String code;
		uint64_t created_at;
		Snowflake guild_id;
		User *inviter;
		int32_t max_age;
		int32_t max_uses;
		int32_t target_type;
		User *target_user;
		Application *target_application;
		bool temporary;
		int32_t uses;
	};

	struct GatewayEventInviteDelete {
		Snowflake channel_id;
		Snowflake guild;
		String code;
	};

	struct GatewayEventMessageCreate {
		Message message;
	};

	struct GatewayEventMessageUpdate {
		Message message;
	};

	struct GatewayEventMessageDelete {
		Snowflake id;
		Snowflake channel_id;
		Snowflake guild_id;
	};

	struct GatewayEventMessageDeleteBulk {
		Array<Snowflake> ids;
		Snowflake channel_id;
		Snowflake guild_id;
	};

	struct GatewayEventMessageReactionAdd {
		Snowflake user_id;
		Snowflake channel_id;
		Snowflake message_id;
		Snowflake guild_id;
		GuildMember *member;
		Emoji emoji;
	};

	struct GatewayEventMessageReactionRemove {
		Snowflake user_id;
		Snowflake channel_id;
		Snowflake message_id;
		Snowflake guild_id;
		Emoji emoji;
	};

	struct GatewayEventMessageReactionRemoveAll {
		Snowflake channel_id;
		Snowflake message_id;
		Snowflake guild_id;
	};

	struct GatewayEventMessageReactionRemoveEmoji {
		Snowflake channel_id;
		Snowflake guild_id;
		Snowflake message_id;
		Emoji emoji;
	};

	struct GatewayEventStageInstanceCreate {
		StageInstance stage_instance;
	};

	struct GatewayEventStageInstanceDelete {
		StageInstance stage_instance;
	};

	struct GatewayEventStageInstanceUpdate {
		StageInstance stage_instance;
	};
	
	struct GatewayEventTypingStart {
		Snowflake channel_id;
		Snowflake guild_id;
		Snowflake user_id;
		int32_t timestamp;
		GuildMember member;
	};

	struct GatewayEventUserUpdate {
		User user;
	};

	struct GatewayEventVoiceStateUpdate {
		VoiceState voice_state;
	};

	struct GatewayEventVoiceServerUpdate {
		String token;
		Snowflake guild_id;
		String endpoint;
	};

	struct GatewayEventWebhooksUpdate {
		Snowflake guild_id;
		Snowflake channel_id;
	};
}
