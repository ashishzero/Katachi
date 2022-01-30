#include "Basic.h"

namespace Discord {
	struct Snowflake {
		uint64_t value;
	};

	//
	// Guild Scheduled Event
	//

	// https://discord.com/developers/docs/resources/guild-scheduled-event

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

	const String GUILD_FEATURE_ANIMATED_ICON = "ANIMATED_ICON";
	const String GUILD_FEATURE_BANNER = "BANNER";
	const String GUILD_FEATURE_COMMERCE = "COMMERCE";
	const String GUILD_FEATURE_COMMUNITY = "COMMUNITY";
	const String GUILD_FEATURE_DISCOVERABLE = "DISCOVERABLE";
	const String GUILD_FEATURE_FEATURABLE = "FEATURABLE";
	const String GUILD_FEATURE_INVITE_SPLASH = "INVITE_SPLASH";
	const String GUILD_FEATURE_MEMBER_VERIFICATION_GATE_ENABLED = "MEMBER_VERIFICATION_GATE_ENABLED";
	const String GUILD_FEATURE_MONETIZATION_ENABLED = "MONETIZATION_ENABLED";
	const String GUILD_FEATURE_MORE_STICKERS = "MORE_STICKERS";
	const String GUILD_FEATURE_NEWS = "NEWS";
	const String GUILD_FEATURE_PARTNERED = "PARTNERED";
	const String GUILD_FEATURE_PREVIEW_ENABLED = "PREVIEW_ENABLED";
	const String GUILD_FEATURE_PRIVATE_THREADS = "PRIVATE_THREADS";
	const String GUILD_FEATURE_ROLE_ICONS = "ROLE_ICONS";
	const String GUILD_FEATURE_SEVEN_DAY_THREAD_ARCHIVE = "SEVEN_DAY_THREAD_ARCHIVE";
	const String GUILD_FEATURE_THREE_DAY_THREAD_ARCHIVE = "THREE_DAY_THREAD_ARCHIVE";
	const String GUILD_FEATURE_TICKETED_EVENTS_ENABLED = "TICKETED_EVENTS_ENABLED";
	const String GUILD_FEATURE_VANITY_URL = "VANITY_URL";
	const String GUILD_FEATURE_VERIFIED = "VERIFIED";
	const String GUILD_FEATURE_VIP_REGIONS = "VIP_REGIONS";
	const String GUILD_FEATURE_WELCOME_SCREEN_ENABLED = "WELCOME_SCREEN_ENABLED";

	struct GuildPreview {
		Snowflake id;
		String name;
		String icon;
		String splash;
		String discovery_splash;
		Array<Emoji> emojis;
		Array<GuildFeature> features;
		int64_t approximate_member_count;
		int64_t approximate_presence_count;
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
		int64_t presence_count;
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
		String pending;
		uint64_t communication_disabled_until;
	};

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
		String description;
		User *bot;
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

	struct Integration {
		Snowflake id;
		String name;
		String type;
		bool enabled;
		bool syncing;
		Snowflake role_id;
		bool enable_emoticons;
		IntegrationExpireBehavior expire_behavior;
		int64_t expire_grace_period;
		User *user;
		IntegrationAccount *account;
		uint64_t synced_at;
		int64_t subscriber_count;
		bool revoked;
		IntegrationApplication *application;
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
		int64_t afk_timeout;
		bool widget_enabled;
		Snowflake widget_channel_id;
		int64_t verification_level;
		int64_t default_message_notifications;
		int64_t explicit_content_filter;
		Array<Role> explicit_content_filter;
		Array<Emoji> emojis;
		Array<GuildFeature> features;
		int64_t mfa_level;
		Snowflake application_id;
		Snowflake system_channel_id;
		int64_t system_channel_flags;
		Snowflake rules_channel_id;
		uint64_t joined_at;
		bool large;
		bool unavailable;
		int64_t member_count;
		Array<VoiceState> voice_states;
		Array<GuildMember> members;
		Array<Channel> channels;
		Array<Channel> threads;
		Array<PresenceUpdate> presences;
		int64_t max_presences;
		int64_t max_members;
		String vanity_url_code;
		String description;
		String banner;
		int64_t premium_tier;
		int64_t premium_subscription_count;
		String preferred_locale;
		Snowflake public_updates_channel_id;
		int64_t max_video_channel_users;
		int64_t approximate_member_count;
		int64_t approximate_presence_count;
		WelcomeScreen *welcome_screen;
		int64_t nsfw_level;
		Array<StageInstance> stage_instances;
		Array<Sticker> stickers;
		Array<GuildScheduledEvent> guild_scheduled_events;
		bool premium_progress_bar_enabled;
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

	struct Channel {
		Snowflake id;
		int64_t type;
		Snowflake guild_id;
		int64_t position;
		Array<Overwrite> permission_overwrites;
		String name;
		String topic;
		bool nsfw;
		Snowflake last_message_id;
		int64_t bitrate;
		int64_t user_limit;
		int64_t rate_limit_per_user;
		Array<User> recipients;
		String icon;
		Snowflake owner_id;
		Snowflake application_id;
		Snowflake parent_id;
		uint64_t last_pin_timestamp;
		String rtc_region;
		int64_t video_quality_mode;
		int64_t message_count;
		int64_t member_count;
		ThreadMetadata thread_metadata;
		ThreadMember member;
		int64_t default_auto_archive_duration;
		String permissions;
	};

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

	inline MessageFlag operator|(MessageFlag a, MessageFlag b) {
		return (MessageFlag)((uint32_t)a | (uint32_t)b);
	}
	inline MessageFlag operator&(MessageFlag a, MessageFlag b) {
		return (MessageFlag)((uint32_t)a & (uint32_t)b);
	}

	struct MessageActivity {
		int64_t type;
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
		int64_t count;
		bool me;
		Emoji emoji;
	};

	struct Overwrite {
		Snowflake id;
		int64_t type;
		String allow;
		String deny;
	};

	struct ThreadMetadata {
		bool archived;
		int64_t auto_archive_duration;
		uint64_t archive_timestamp;
		bool locked;
		bool invitable;
		uint64_t create_timestamp;
	};

	struct ThreadMember {
		Snowflake id;
		Snowflake user_id;
		uint64_t join_timestamp;
		int64_t flags;
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
		int64_t height;
		int64_t width;
	};

	struct EmbedVideo {
		String url;
		String proxy_url;
		int64_t height;
		int64_t width;
	};

	struct EmbedImage {
		String url;
		String proxy_url;
		int64_t height;
		int64_t width;
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
		int64_t color;
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
		int64_t size;
		String url;
		String proxy_url;
		int64_t height;
		int64_t width;
		bool ephemeral;
	};

	struct ChannelMention {
		Snowflake id;
		Snowflake guild_id;
		int64_t type;
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
		int64_t type;
		MessageActivity *activity;
		struct Application *application;
		Snowflake application_id;
		MessageReference *message_reference;
		int64_t flags;
		MessageObject *referenced_message;
		MessageInteractionObject *interaction;
		Channel *thread;
		Array<MessageComponent> components;
		Array<MessageStickerItem> sticker_items;
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
		Team *team;
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

	inline ApplicationFlag operator |(ApplicationFlag a, ApplicationFlag b) { 
		return (ApplicationFlag)((uint32_t)a | (uint32_t)b);
	}
	inline ApplicationFlag operator &(ApplicationFlag a, ApplicationFlag b) { 
		return (ApplicationFlag)((uint32_t)a & (uint32_t)b);
	}


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
		int64_t afk_timeout;
		String allow;
		Snowflake application_id;
		bool archived;
		String asset;
		int64_t auto_archive_duration;
		bool available;
		String avatar_hash;
		String banner_hash;
		int64_t bitrate;
		Snowflake channel_id;
		String code;
		int64_t color;
		uint64_t communication_disabled_until;
		bool deaf;
		int64_t default_auto_archive_duration;
		int64_t default_message_notifications;
		String deny;
		String description;
		String discovery_splash_hash;
		bool enable_emoticons;
		int64_t entity_type;
		int64_t expire_behavior;
		int64_t expire_grace_period;
		int64_t explicit_content_filter;
		FormatType format_type;
		Snowflake guild_id;
		bool hoist;
		String icon_hash;
		Snowflake id;
		bool invitable;
		Snowflake inviter_id;
		String location;
		bool locked;
		int64_t max_age;
		int64_t max_uses;
		bool mentionable;
		int64_t mfa_level;
		bool mute;
		String name;
		String nick;
		bool nsfw;
		Snowflake owner_id;
		Array<ChannelOverwrite> permission_overwrites;
		String permissions;
		int64_t position;
		String preferred_locale;
		PrivacyLevel privacy_level;
		int64_t prune_delete_days;
		Snowflake public_updates_channel_id;
		int64_t rate_limit_per_user;
		String region;
		Snowflake rules_channel_id;
		String splash_hash;
		Status status;
		Snowflake system_channel_id;
		String tags;
		bool temporary;
		String topic;
		String type;
		String unicode_emoji;
		int64_t user_limit;
		int64_t uses;
		String vanity_url_code;
		int64_t verification_level;
		Snowflake widget_channel_id;
		bool widget_enabled;
		Array<Role> add;
		Array<Role> remove;
	};

	struct AuditLogChange {
		AuditLogChangeKey *new_value; // TODO: Verify this
		AuditLogChangeKey *old_value; // TODO: Verify this
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
}
