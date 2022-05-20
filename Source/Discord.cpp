#include "Discord.h"

#include "Kr/KrString.h"
#include "Kr/KrThread.h"

#include "Websocket.h"
#include "Json.h"

#include <stdlib.h>
#include <math.h>
#include <time.h>

constexpr int DISCORD_HTTP_SEND_BUFFER_SIZE    = MegaBytes(16);
constexpr int DISCORD_HTTP_RECEIVE_BUFFER_SIZE = MegaBytes(16);

namespace Discord {
	const String UserAgent        = "Katachi (https://github.com/Zero5620/Katachi, 0.1.1)";
	const String BaseHttpUrl = "/api/v10";
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

static void Discord_Jsonify(const Discord::Overwrite &overwrite, Jsonify *j) {
	j->BeginObject();
	j->KeyValue("id", overwrite.id.value);
	j->KeyValue("type", (int)overwrite.type);
	j->KeyValue("allow", overwrite.allow);
	j->KeyValue("deny", overwrite.deny);
	j->EndObject();
}

//
//
//

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

	long years  = strtol(buffer + 0, nullptr, 10);
	long months = strtol(buffer + 5, nullptr, 10);
	long days   = strtol(buffer + 8, nullptr, 10);
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

//
//
//

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
	channel->video_quality_mode            = (Discord::VideoQualityMode)JsonGetInt(obj, "video_quality_mode", 1);
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
			option->value.string = value->value.string.value;
		} else if (value->type == JSON_TYPE_NUMBER) {
			option->value.number = value->value.number.real;
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

static void Discord_SetupEventHandlers(Discord::EventHandler *h) {
	using namespace Discord;
	if (!h->tick) h->tick           = [](Client *) {};
	if (!h->hello) h->hello         = [](Client *, int32_t) {};
	if (!h->ready) h->ready         = [](Client *, const Ready &) {};
	if (!h->resumed) h->resumed     = [](Client *) {};
	if (!h->reconnect) h->reconnect = [](Client *) {};
	if (!h->invalid_session) h->invalid_session = [](Discord::Client *, bool) {};
	if (!h->application_command_permissions_update) h->application_command_permissions_update = [](Client *, const ApplicationCommandPermissions &) {};
	if (!h->channel_create) h->channel_create = [](Client *, const Channel &channel) {};
	if (!h->channel_update) h->channel_update = [](Client *, const Channel &channel) {};
	if (!h->channel_delete) h->channel_delete = [](Client *, const Channel &channel) {};
	if (!h->channel_pins_update) h->channel_pins_update = [](Client *, Snowflake guild_id, Snowflake channel_id, Timestamp last_pin_timestamp) {};
	if (!h->thread_create) h->thread_create = [](Client *, const Channel &channel, bool newly_created) {};
	if (!h->thread_update) h->thread_update = [](Client *, const Channel &channel) {};
	if (!h->thread_delete) h->thread_delete = [](Client *, Snowflake id, Snowflake guild_id, Snowflake parent_id, ChannelType type) {};
	if (!h->thread_list_sync) h->thread_list_sync = [](Client *, Snowflake guild_id, Array_View<Snowflake> channel_ids, Array_View<Channel> threads, Array<ThreadMember> members) {};
	if (!h->thread_member_update) h->thread_member_update = [](Client *, Snowflake guild_id, const ThreadMember & member) {};
	if (!h->thread_members_update) h->thread_members_update = [](Client *, Snowflake id, Snowflake guild_id, int32_t member_count, Array_View<ThreadMember> added_members, Array_View<Snowflake> removed_member_ids) {};
	if (!h->guild_create) h->guild_create = [](Client *, const Guild &guild, const GuildInfo &info) {};
	if (!h->guild_update) h->guild_update = [](Client *, const Guild &guild) {};
	if (!h->guild_delete) h->guild_delete = [](Client *, const UnavailableGuild &unavailable_guild) {};
	if (!h->guild_ban_add) h->guild_ban_add = [](Client *, Snowflake guild_id, const User &user) {};
	if (!h->guild_ban_remove) h->guild_ban_remove = [](Client *, Snowflake guild_id, const User &user) {};
	if (!h->guild_emojis_update) h->guild_emojis_update = [](Client *, Snowflake guild_id, Array_View<Emoji> emojis) {};
	if (!h->guild_stickers_update) h->guild_stickers_update = [](Client *, Snowflake guild_id, Array_View<Sticker> stickers) {};
	if (!h->guild_integrations_update) h->guild_integrations_update = [](Client *, Snowflake guild_id) {};
	if (!h->guild_member_add) h->guild_member_add = [](Client *, Snowflake guild_id, const GuildMember &member) {};
	if (!h->guild_member_remove) h->guild_member_remove = [](Client *, Snowflake guild_id, const User &user) {};
	if (!h->guild_member_update) h->guild_member_update = [](Client *, Snowflake guild_id, const GuildMemberUpdate &member) {};
	if (!h->guild_members_chunk) h->guild_members_chunk = [](Client *, Snowflake guild_id, const GuildMembersChunk &member_chunk) {};
	if (!h->guild_role_create) h->guild_role_create = [](Client *, Snowflake guild_id, const Role &role) {};
	if (!h->guild_role_update) h->guild_role_update = [](Client *, Snowflake guild_id, const Role &role) {};
	if (!h->guild_role_delete) h->guild_role_delete = [](Client *, Snowflake guild_id, Snowflake role_id) {};
	if (!h->guild_scheduled_event_create) h->guild_scheduled_event_create = [](Client *, const GuildScheduledEvent &scheduled_event) {};
	if (!h->guild_scheduled_event_update) h->guild_scheduled_event_update = [](Client *, const GuildScheduledEvent &scheduled_event) {};
	if (!h->guild_scheduled_event_delete) h->guild_scheduled_event_delete = [](Client *, const GuildScheduledEvent &scheduled_event) {};
	if (!h->guild_scheduled_event_user_add) h->guild_scheduled_event_user_add = [](Client *, Snowflake guild_scheduled_event_id, Snowflake user_id, Snowflake guild_id) {};
	if (!h->guild_scheduled_event_user_remove) h->guild_scheduled_event_user_remove = [](Client *, Snowflake guild_scheduled_event_id, Snowflake user_id, Snowflake guild_id) {};
	if (!h->integration_create) h->integration_create = [](Client *, Snowflake guild_id, const Integration &integration) {};
	if (!h->integration_update) h->integration_update = [](Client *, Snowflake guild_id, const Integration &integration) {};
	if (!h->integration_delete) h->integration_delete = [](Client *, Snowflake id, Snowflake guild_id, Snowflake application_id) {};
	if (!h->interaction_create) h->interaction_create = [](Client *, const Interaction &interaction) {};
	if (!h->invite_create) h->invite_create = [](Client *, const InviteInfo &invite) {};
	if (!h->invite_delete) h->invite_delete = [](Client *, Snowflake channel_id, Snowflake guild_id, String code) {};
	if (!h->message_create) h->message_create = [](Client *, const Message &message) {};
	if (!h->message_update) h->message_update = [](Client *, const Message &message) {};
	if (!h->message_delete) h->message_delete = [](Client *, Snowflake id, Snowflake channel_id, Snowflake guild_id) {};
	if (!h->message_delete_bulk) h->message_delete_bulk = [](Client *, Array_View<Snowflake> ids, Snowflake channel_id, Snowflake guild_id) {};
	if (!h->message_reaction_add) h->message_reaction_add = [](Client *, const MessageReactionInfo &reaction) {};
	if (!h->message_reaction_remove) h->message_reaction_remove = [](Client *, Snowflake user_id, Snowflake channel_id, Snowflake message_id, Snowflake guild_id, const Emoji &emoji) {};
	if (!h->message_reaction_remove_all) h->message_reaction_remove_all = [](Client *, Snowflake channel_id, Snowflake message_id, Snowflake guild_id) {};
	if (!h->message_reaction_remove_emoji) h->message_reaction_remove_emoji = [](Client *, Snowflake channel_id, Snowflake guild_id, Snowflake message_id, const Emoji &emoji) {};
	if (!h->presence_update) h->presence_update = [](Client *, const Presence &presence) {};
	if (!h->stage_instance_create) h->stage_instance_create = [](Client *, StageInstance &stage) {};
	if (!h->stage_instance_delete) h->stage_instance_delete = [](Client *, StageInstance &stage) {};
	if(!h->typing_start) h->typing_start = [](Client *, const TypingStartInfo &typing) {};
	if (!h->user_update) h->user_update = [](Client *, const User &user) {};
	if (!h->voice_state_update) h->voice_state_update = [](Client *, const VoiceState &voice_state) {};
	if (!h->voice_server_update) h->voice_server_update = [](Client *, String token, Snowflake guild_id, String endpoint) {};
	if (!h->webhooks_update) h->webhooks_update = [](Client *, Snowflake guild_id, Snowflake channel_id) {};
}

//
//
//

struct Discord_GatewayResponse {
	int32_t shards;
	struct {
		int32_t total;
		int32_t remaining;
		int32_t reset_after;
		int32_t max_concurrency;
	} session_start_limit;
};

static bool Discord_ConnectToGatewayBot(String token, Memory_Arena *arena, Discord_GatewayResponse *response) {
	auto temp = BeginTemporaryMemory(arena);
	Defer{ EndTemporaryMemory(&temp); };

	String authorization = FmtStr(arena, "Bot " StrFmt, StrArg(token));

	Http *http = Http_Connect("https://discord.com", HTTPS_CONNECTION, MemoryArenaAllocator(arena));
	if (!http)
		return nullptr;

	Http_Request req;
	Http_InitRequest(&req);
	Http_SetHost(&req, http);
	Http_SetHeader(&req, HTTP_HEADER_AUTHORIZATION, authorization);
	Http_SetHeader(&req, HTTP_HEADER_USER_AGENT, Discord::UserAgent);

	String endpoint = FmtStr(arena, StrFmt "/gateway/bot", StrArg(Discord::BaseHttpUrl));

	Http_Response res;
	if (!Http_Get(http, endpoint, req, &res, arena)) {
		Http_Disconnect(http);
		return nullptr;
	}

	Json json;
	if (!JsonParse(res.body, &json, MemoryArenaAllocator(arena))) {
		LogErrorEx("Discord", "Failed to parse JSON response: \n" StrFmt, StrArg(res.body));
		Http_Disconnect(http);
		return false;
	}

	const Json_Object obj = JsonGetObject(json);

	if (res.status.code != 200) {
		String msg = JsonGetString(obj, "message");
		LogErrorEx("Discord", "Connection Error; Code: %u, Message: " StrFmt, res.status.code, StrArg(msg));
		Http_Disconnect(http);
		return false;
	}

	Http_Disconnect(http);

	response->shards = JsonGetInt(obj, "shards");

	Json_Object session_start_limit               = JsonGetObject(obj, "session_start_limit");
	response->session_start_limit.total           = JsonGetInt(session_start_limit, "total");
	response->session_start_limit.remaining       = JsonGetInt(session_start_limit, "remaining");
	response->session_start_limit.reset_after     = JsonGetInt(session_start_limit, "reset_after");
	response->session_start_limit.max_concurrency = JsonGetInt(session_start_limit, "max_concurrency");

	return true;
}

static Websocket *Discord_ConnectToGateway(String token, Memory_Arena *scratch, Memory_Allocator allocator, Websocket_Spec spec) {
	auto temp = BeginTemporaryMemory(scratch);
	Defer{ EndTemporaryMemory(&temp); };

	Http *http = Http_Connect("https://discord.com", HTTPS_CONNECTION, MemoryArenaAllocator(scratch));
	if (!http)
		return nullptr;

	Http_Request req;
	Http_InitRequest(&req);
	Http_SetHost(&req, http);
	Http_SetHeader(&req, HTTP_HEADER_USER_AGENT, Discord::UserAgent);

	String endpoint = FmtStr(scratch, StrFmt "/gateway", StrArg(Discord::BaseHttpUrl));

	Http_Response res;
	if (!Http_Get(http, endpoint, req, &res, scratch)) {
		Http_Disconnect(http);
		return nullptr;
	}

	Json json;
	if (!JsonParse(res.body, &json, MemoryArenaAllocator(scratch))) {
		LogErrorEx("Discord", "Failed to parse JSON response: \n" StrFmt, StrArg(res.body));
		Http_Disconnect(http);
		return nullptr;
	}

	const Json_Object obj = JsonGetObject(json);

	if (res.status.code != 200) {
		String msg = JsonGetString(obj, "message");
		LogErrorEx("Discord", "Connection Error; Code: %u, Message: " StrFmt, res.status.code, StrArg(msg));
		Http_Disconnect(http);
		return nullptr;
	}

	Http_Disconnect(http);

	String url = JsonGetString(obj, "url");

	String authorization = FmtStr(scratch, "Bot " StrFmt, StrArg(token));

	Websocket_Header headers;
	Websocket_InitHeader(&headers);
	Websocket_HeaderSet(&headers, HTTP_HEADER_AUTHORIZATION, authorization);
	Websocket_HeaderSet(&headers, HTTP_HEADER_USER_AGENT, Discord::UserAgent);
	Websocket_QueryParamSet(&headers, "v", "9");
	Websocket_QueryParamSet(&headers, "encoding", "json");

	Websocket *websocket = Websocket_Connect(url, &res, &headers, spec, allocator);
	return websocket;
}

struct Discord_ShardThread {
	Thread *                 handle;
	String                   token;
	int                      intents;
	Discord::EventHandler    onevent;
	Discord::PresenceUpdate *presence;
	Discord::ClientSpec      spec;
};

static int Discord_ShardThreadProc(void *arg) {
	Discord_ShardThread *shard = (Discord_ShardThread *)arg;
	Discord::Login(shard->token, shard->intents, shard->onevent, shard->presence, shard->spec);
	return 0;
}

//
//
//

static void Discord_HandleWebsocketEvent(Discord::Client *client, const Websocket_Event &event);

static bool Discord_CustomMethod(Discord::Client *client, const String method, const String api_endpoint, const Http_Query_Params &params, const String body, Json *json);

static bool Discord_CustomMethod(Discord::Client *client, const String method, const String api_endpoint, const String body, Json *res) {
	Http_Query_Params params;
	return Discord_CustomMethod(client, method, api_endpoint, params, body, res);
}

static inline bool Discord_Get(Discord::Client *client, const String api_endpoint, const Http_Query_Params &params, const String body, Json *res) {
	return Discord_CustomMethod(client, "GET", api_endpoint, params, body, res);
}

static inline bool Discord_Post(Discord::Client *client, const String api_endpoint, const Http_Query_Params &params, const String body, Json *res) {
	return Discord_CustomMethod(client, "POST", api_endpoint, params, body, res);
}

static inline bool Discord_Patch(Discord::Client *client, const String api_endpoint, const Http_Query_Params &params, const String body, Json *res) {
	return Discord_CustomMethod(client, "PATCH", api_endpoint, params, body, res);
}

static inline bool Discord_Delete(Discord::Client *client, const String api_endpoint, const Http_Query_Params &params, const String body, Json *res) {
	return Discord_CustomMethod(client, "DELETE", api_endpoint, params, body, res);
}

static inline bool Discord_Get(Discord::Client *client, const String api_endpoint, const String body, Json *res) {
	return Discord_CustomMethod(client, "GET", api_endpoint, body, res);
}

static inline bool Discord_Post(Discord::Client *client, const String api_endpoint, const String body, Json *res) {
	return Discord_CustomMethod(client, "POST", api_endpoint, body, res);
}

static inline bool Discord_Patch(Discord::Client *client, const String api_endpoint, const String body, Json *res) {
	return Discord_CustomMethod(client, "PATCH", api_endpoint, body, res);
}

static inline bool Discord_Delete(Discord::Client *client, const String api_endpoint, const String body, Json *res) {
	return Discord_CustomMethod(client, "DELETE", api_endpoint, body, res);
}

//
//
//

namespace Discord {
	
	struct Client {
		Websocket *      websocket = nullptr;
		Heartbeat        heartbeat;

		Http *           http = nullptr;
		String           authorization;

		EventHandler     onevent;

		Memory_Arena *   scratch   = nullptr;
		Memory_Allocator allocator = ThreadContext.allocator;

		Identify         identify;
		uint8_t          session_id[1024] = {0};
		int              sequence = -1;
		bool             login = false;
	};

	void IdentifyCommand(Client *client) {
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Opcode::IDENTIFY);
		j.PushKey("d");
		Discord_Jsonify(client->identify, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void ResumeCommand(Client *client) {
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Opcode::RESUME);
		j.PushKey("d");
		j.BeginObject();
		j.KeyValue("token", client->identify.token);
		j.KeyValue("session_id", String(client->session_id, strlen((char *)client->session_id)));
		if (client->sequence >= 0)
			j.KeyValue("seq", client->sequence);
		else
			j.KeyNull("seq");
		j.EndObject();
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void HearbeatCommand(Client *client) {
		if (client->heartbeat.count != client->heartbeat.acknowledged) {
			LogWarningEx("Discord", "No Acknowledgement %d", client->heartbeat.acknowledged);
			Websocket_Close(client->websocket, WEBSOCKET_CLOSE_ABNORMAL_CLOSURE);
			return;
		}

		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Opcode::HEARTBEAT);
		if (client->sequence >= 0)
			j.KeyValue("d", client->sequence);
		else
			j.KeyNull("d");
		j.EndObject();

		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
		client->heartbeat.count += 1;
	}

	void GuildMembersRequestCommand(Client *client, const GuildMembersRequest &req_guild_mems) {
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Opcode::REQUEST_GUILD_MEMBERS);
		j.PushKey("d");
		Discord_Jsonify(req_guild_mems, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void VoiceStateUpdateCommand(Client *client, const VoiceStateUpdate &update_voice_state) {
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Opcode::UPDATE_VOICE_STATE);
		j.PushKey("d");
		Discord_Jsonify(update_voice_state, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void PresenceUpdateCommand(Client *client, const PresenceUpdate &presence_update) {
		Jsonify j(client->scratch);
		j.BeginObject();
		j.KeyValue("op", (int)Opcode::UPDATE_PRESECE);
		j.PushKey("d");
		Discord_Jsonify(presence_update, &j);
		j.EndObject();
		String msg = Jsonify_BuildString(&j);
		Websocket_SendText(client->websocket, msg);
	}

	void Login(const String token, int32_t intents, EventHandler onevent, PresenceUpdate *presence, ClientSpec spec) {
		Assert(spec.tick_ms >= 0);

		constexpr ClientSpec DefaultClientSpec = ClientSpec();
		
		int tick          = spec.tick_ms;
		spec.scratch_size = Maximum(spec.scratch_size, DefaultClientSpec.scratch_size);
		spec.read_size    = Maximum(spec.read_size,    DefaultClientSpec.read_size);
		spec.write_size   = Maximum(spec.write_size,   DefaultClientSpec.write_size);
		spec.queue_size   = Maximum(spec.queue_size,   DefaultClientSpec.queue_size);

		Websocket_Spec websocket_spec;
		websocket_spec.read_size  = spec.read_size;
		websocket_spec.write_size = spec.write_size;
		websocket_spec.queue_size = spec.queue_size;

		Memory_Arena *arena = MemoryArenaAllocate(spec.scratch_size);
		Defer{ if (arena) MemoryArenaFree(arena); };

		if (!arena) {
			LogErrorEx("Discord", "Memory allocation failed");
			return;
		}

		Discord::Client client;
		client.scratch    = arena;
		client.allocator  = spec.allocator;
		client.identify   = Discord::Identify(token, intents, presence);
		client.onevent    = onevent;
		client.login      = true;

		client.identify.shard[0] = spec.shards[0];
		client.identify.shard[1] = spec.shards[1];

		client.identify.properties.browser = "Katachi";
		client.identify.properties.device  = "Katachi";

		client.authorization = FmtStr(client.allocator, "Bot " StrFmt, StrArg(client.identify.token));

		ThreadContext.allocator = MemoryArenaAllocator(arena);

		Discord_SetupEventHandlers(&client.onevent);

		while (client.login) {
			client.heartbeat = Discord::Heartbeat();

			for (int reconnect = 0; !client.websocket; ++reconnect) {
				client.websocket = Discord_ConnectToGateway(token, arena, spec.allocator, websocket_spec);
				if (!client.websocket) {
					int maximum_backoff = 32; // secs
					int wait_time = Minimum((int)powf(2.0f, (float)reconnect), maximum_backoff);
					LogInfoEx("Discord", "Reconnect after %d secs...", wait_time);
					wait_time = wait_time * 1000 + rand() % 1000; // to ms
					Thread_Sleep(wait_time);
					LogInfoEx("Discord", "Reconnecting...");
				}
			}

			clock_t counter            = clock();
			client.heartbeat.remaining = client.heartbeat.interval;

			while (Websocket_IsConnected(client.websocket)) {
				Websocket_Event event;
				Websocket_Result res = Websocket_Receive(client.websocket, &event, client.scratch, tick);

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
					Discord::HearbeatCommand(&client);
					TraceEx("Discord", "Heartbeat (%d)", client.heartbeat.count);
				}

				if (res == WEBSOCKET_E_WAIT) {
					client.onevent.tick(&client);
				}

				MemoryArenaReset(client.scratch);
			}

			Websocket_Disconnect(client.websocket);
			client.websocket = nullptr;
		}
	}

	void LoginSharded(const String token, int32_t intents, EventHandler onevent, PresenceUpdate *presence, int32_t shard_count, const ShardSpec &specs) {
		Memory_Arena *arena = MemoryArenaAllocate(KiloBytes(128));

		Discord_GatewayResponse response;

		for (int reconnect = 0; ; ++reconnect) {
			if (Discord_ConnectToGatewayBot(token, arena, &response))
				break;

			int maximum_backoff = 32; // secs
			int wait_time = Minimum((int)powf(2.0f, (float)reconnect), maximum_backoff);
			LogInfoEx("Discord", "Reconnect after %d secs...", wait_time);
			wait_time = wait_time * 1000 + rand() % 1000; // to ms
			Thread_Sleep(wait_time);
			LogInfoEx("Discord", "Reconnecting...");
		}

		if (shard_count <= 0) {
			shard_count = response.shards;
		}

		Thread_Context_Params params = ThreadContextDefaultParams;
		params.logger                = ThreadContext.logger;

		Discord_ShardThread *shards = PushArray(arena, Discord_ShardThread, shard_count);
		if (!shards) {
			MemoryArenaFree(arena);
			LogErrorEx("Discord", "Failed to allocate memory to launch shards");
			return;
		}

		TraceEx("Discord", "Shard count: %d", shard_count);

		int max_concurrency = response.session_start_limit.max_concurrency;

		for (int32_t shard_id = 0; shard_id < shard_count - 1; ++shard_id) {
			Discord_ShardThread *shard = &shards[shard_id];
			shard->token               = token;
			shard->intents             = intents;
			shard->onevent             = onevent;
			shard->presence            = nullptr;

			if (shard_id < specs.specs.count) {
				shard->spec = specs.specs[shard_id];
			} else {
				shard->spec = specs.default_spec;
				shard->spec.shards[0] = shard_id;
				shard->spec.shards[1] = shard_count;
			}

			shard->handle = Thread_Create(Discord_ShardThreadProc, shard, 0, params);

			int rate_limit_key = shard_id % max_concurrency;
			if (!shard_id && !rate_limit_key) {
				Thread_Sleep(5000);
			}
		}

		int32_t shard_id = shard_count - 1;
		Discord_ShardThread *shard = &shards[shard_id];

		shard->token    = token;
		shard->intents  = intents;
		shard->onevent  = onevent;
		shard->presence = presence;

		if (shard_id < specs.specs.count) {
			shard->spec = specs.specs[shard_id];
		} else {
			shards[shard_id].spec = specs.default_spec;
			shard->spec.shards[0] = shard_id;
			shard->spec.shards[1] = shard_count;
		}
		
		shard->handle = nullptr;

		Discord_ShardThreadProc(shard);

		for (int32_t shard_id = 0; shard_id < shard_count; ++shard_id) {
			Thread_Wait(shards[shard_id].handle, -1);
		}

		for (int32_t shard_id = 0; shard_id < shard_count; ++shard_id) {
			Thread_Destroy(shards[shard_id].handle);
		}

		MemoryArenaFree(arena);
	}

	void Logout(Client *client) {
		if (client->login) {
			LogInfoEx("Discord", "Logging out...");
			Websocket_Close(client->websocket, WEBSOCKET_CLOSE_NORMAL);
			client->login = false;
		}
	}

	Shard GetShard(Client *client) {
		return { client->identify.shard[0], client->identify.shard[1] };
	}

	void Initialize() {
		Net_Initialize();
		srand((unsigned int)time(0));
	}

	Channel *GetChannel(Client *client, Snowflake channel_id) {
		String endpoint = FmtStr(client->scratch, "/channels/%zu", channel_id);

		Json res;
		if (Discord_Get(client, endpoint, String(), &res)) {
			Channel *channel = new Channel;
			Discord_Deserialize(JsonGetObject(res), channel);
			return channel;
		}
		return nullptr;
	}

	Channel *ModifyChannel(Client *client, Snowflake channel_id, const ChannelPatch &patch) {
		Jsonify j(client->scratch);

		j.BeginObject();

		if (patch.name.length)
			j.KeyValue("name", patch.name);

		if (patch.icon.length)
			j.KeyValue("icon", patch.icon);

		if (patch.type)
			j.KeyValue("type", (int)*patch.type);

		if (patch.position)
			j.KeyValue("position", *patch.position);

		if (patch.topic.length)
			j.KeyValue("topic", patch.topic);

		if (patch.nsfw)
			j.KeyValue("nsfw", *patch.nsfw);

		if (patch.rate_limit_per_user)
			j.KeyValue("rate_limit_per_user", *patch.rate_limit_per_user);

		if (patch.bitrate)
			j.KeyValue("bitrate", *patch.bitrate);

		if (patch.user_limit)
			j.KeyValue("user_limit", *patch.user_limit);

		if (patch.permission_overwrites.count) {
			j.PushKey("permission_overwrites");
			j.BeginArray();
			for (const auto &overwrite : patch.permission_overwrites) {
				Discord_Jsonify(overwrite, &j);
			}
			j.EndArray();
		}

		if (patch.parent_id.value)
			j.KeyValue("parent_id", patch.parent_id.value);

		if (patch.rtc_region) {
			if (patch.rtc_region->length)
				j.KeyValue("rtc_region", *patch.rtc_region);
			else
				j.KeyNull("rtc_region");
		}

		if (patch.video_quality_mode)
			j.KeyValue("video_quality_mode", *patch.video_quality_mode);

		if (patch.default_auto_archive_duration)
			j.KeyValue("default_auto_archive_duration", *patch.default_auto_archive_duration);

		j.EndObject();

		String endpoint = FmtStr(client->scratch, "/channels/%zu", channel_id);
		String body     = Jsonify_BuildString(&j);

		Json res;
		if (Discord_Patch(client, endpoint, body, &res)) {
			Channel *channel = new Channel;
			Discord_Deserialize(JsonGetObject(res), channel);
			return channel;
		}
		return nullptr;
	}

	Channel *DeleteChannel(Client *client, Snowflake channel_id) {
		String endpoint = FmtStr(client->scratch, "/channels/%zu", channel_id);

		Json res;
		if (Discord_Delete(client, endpoint, String(), &res)) {
			Channel *channel = new Channel;
			Discord_Deserialize(JsonGetObject(res), channel);
			return channel;
		}
		return nullptr;
	}

	Array_View<Message> GetChannelMessages(Client *client, Snowflake channel_id, int limit, Snowflake around, Snowflake before, Snowflake after) {
		String endpoint = FmtStr(client->scratch, "/channels/%zu/messages", channel_id);

		Http_Query_Params params;
		if (limit > 0)
			Http_QueryParamSet(&params, "limit", FmtStr(client->scratch, "%d", limit));
		if (around)
			Http_QueryParamSet(&params, "limit", FmtStr(client->scratch, "%zu", around.value));
		else if (before)
			Http_QueryParamSet(&params, "limit", FmtStr(client->scratch, "%zu", before.value));
		else if (after)
			Http_QueryParamSet(&params, "limit", FmtStr(client->scratch, "%zu", after.value));

		Json res;
		if (Discord_Get(client, endpoint, params, String(), &res)) {
			Json_Array arr = JsonGetArray(res);
			Array<Message> messages;
			messages.Resize(arr.count);
			for (ptrdiff_t index = 0; index < messages.count; ++index) {
				Discord_Deserialize(JsonGetObject(arr[index]), &messages[index]);
			}
			return messages;
		}
		return Array_View<Message>();
	}

	Message *GetChannelMessage(Client *client, Snowflake channel_id, Snowflake message_id) {
		String endpoint = FmtStr(client->scratch, "/channels/%zu/messages/%zu", channel_id, message_id);

		Json res;
		if (Discord_Get(client, endpoint, String(), &res)) {
			Message *message = new Message;
			Discord_Deserialize(JsonGetObject(res), message);
			return message;
		}
		return nullptr;
	}
}

//
//
//

static void Discord_InitHttpRequest(Http *http, Http_Request *req, String authorization, String body = String()) {
	Http_InitRequest(req);
	Http_SetHost(req, http);
	Http_SetHeader(req, HTTP_HEADER_CONNECTION, "close");
	Http_SetHeader(req, HTTP_HEADER_USER_AGENT, Discord::UserAgent);
	Http_SetHeader(req, HTTP_HEADER_AUTHORIZATION, authorization);
	Http_SetContent(req, "application/json", body);
}

static bool Discord_HttpConnect(Discord::Client *client) {
	const String host = "https://discord.com";
	client->http = Http_Connect(host, HTTPS_CONNECTION, client->allocator);
	if (!client->http) {
		LogErrorEx("Discord", "Unable to connect to \"" StrFmt "\".", StrArg(host));
		return nullptr;
	}

	return true;
}

static bool Discord_CustomMethod(Discord::Client *client, const String method, const String api_endpoint, const Http_Query_Params &params, const String body, Json *json) {
	if (!client->http) {
		if (!Discord_HttpConnect(client))
			return false;
	}

	String endpoint = FmtStr(client->scratch, StrFmt StrFmt, StrArg(Discord::BaseHttpUrl), StrArg(api_endpoint));

	Http_Request req;
	Http_Response res;

	for (int retry = 0; retry < 2; ++retry) {
		Discord_InitHttpRequest(client->http, &req, client->authorization, body);
		if (Http_CustomMethod(client->http, method, endpoint, params, req, &res, client->scratch)) {
			if (res.status.code != 200) {
				// @todo: handle rate limiting
				return false;
			}

			if (JsonParse(res.body, json))
				return true;

			LogErrorEx("Discord", "Failed to parse HTTP response");

			return false;
		} else {
			Http_Disconnect(client->http);
			if (!Discord_HttpConnect(client))
				return false;
		}
	}

	return false;
}

//
//
//

typedef void(*Discord_Event_Handler)(Discord::Client *client, const Json &data);

static void Discord_EventHandlerNone(Discord::Client *client, const Json &data) {}

static void Discord_EventHandlerHello(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	client->heartbeat.interval = JsonGetFloat(obj, "heartbeat_interval", 45000);
	client->onevent.hello(client, (int32_t)client->heartbeat.interval);
}

static void Discord_EventHandlerReady(Discord::Client *client, const Json &data) {
	Discord::Ready ready;

	Json_Object obj = JsonGetObject(data);
	ready.v = JsonGetInt(obj, "v");
	Discord_Deserialize(JsonGetObject(obj, "user"), &ready.user);

	Json_Array jsonguilds = JsonGetArray(obj, "guilds");

	Array<Discord::UnavailableGuild> guilds;
	guilds.Resize(jsonguilds.count);

	for (ptrdiff_t index = 0; index < ready.guilds.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonguilds[index]), &guilds[index]);
	}

	ready.guilds = guilds;

	ready.session_id = JsonGetString(obj, "session_id");

	Json_Array shard = JsonGetArray(obj, "shard");
	ready.shard[0]   = JsonGetInt(shard[0], 0);
	ready.shard[1]   = JsonGetInt(shard[1], 1);

	Json_Object application = JsonGetObject(obj, "application");
	ready.application.id    = Discord_ParseId(JsonGetString(application, "id"));
	ready.application.flags = JsonGetInt(application, "flags");

	Assert(ready.session_id.length < sizeof(client->session_id));
	memset(client->session_id, 0, sizeof(client->session_id));
	memcpy(client->session_id, ready.session_id.data, ready.session_id.length);

	TraceEx("Discord", "Client Session ready: " StrFmt, StrArg(ready.session_id));

	client->onevent.ready(client, ready);
}

static void Discord_EventHandlerResumed(Discord::Client *client, const Json &data) {
	client->onevent.resumed(client);
}

static void Discord_EventHandlerReconnect(Discord::Client *client, const Json &data) {
	client->onevent.reconnect(client);
	// Abnormal closure so that we can resume the session
	Websocket_Close(client->websocket, WEBSOCKET_CLOSE_ABNORMAL_CLOSURE);
}

static void Discord_EventHandlerInvalidSession(Discord::Client *client, const Json &data) {
	bool resumable = JsonGetBool(data);
	client->onevent.invalid_session(client, resumable);

	// If session_id is present, then resume was requested
	if (strlen((char *)client->session_id)) {
		if (!resumable) {
			TraceEx("Discord", "Failed to resume session: %s", client->session_id);

			// Clean session_id so that identfy payload is sent if disconnected
			memset(client->session_id, 0, sizeof(client->session_id));

			Websocket_Close(client->websocket, WEBSOCKET_CLOSE_ABNORMAL_CLOSURE);

			int wait_time = rand() % 4 + 1;
			Thread_Sleep(wait_time * 1000);
		}
	} else {
		Websocket_Close(client->websocket, WEBSOCKET_CLOSE_ABNORMAL_CLOSURE);
	}
}

static void Discord_EventHandlerApplicationCommandPermissionsUpdate(Discord::Client *client, const Json &data) {
	Discord::ApplicationCommandPermissions permissions;
	Discord_Deserialize(JsonGetObject(data), &permissions);
	client->onevent.application_command_permissions_update(client, permissions);
}

static void Discord_EventHandlerChannelCreate(Discord::Client *client, const Json &data) {
	Discord::Channel channel;
	Discord_Deserialize(JsonGetObject(data), &channel);
	client->onevent.channel_create(client, channel);
}

static void Discord_EventHandlerChannelUpdate(Discord::Client *client, const Json &data) {
	Discord::Channel channel;
	Discord_Deserialize(JsonGetObject(data), &channel);
	client->onevent.channel_update(client, channel);
}

static void Discord_EventHandlerChannelDelete(Discord::Client *client, const Json &data) {
	Discord::Channel channel;
	Discord_Deserialize(JsonGetObject(data), &channel);
	client->onevent.channel_delete(client, channel);
}

static void Discord_EventHandlerChannelPinsUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj                       = JsonGetObject(data);
	Discord::Snowflake guild_id           = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Snowflake channel_id         = Discord_ParseId(JsonGetString(obj, "channel_id"));
	Discord::Timestamp last_pin_timestamp = Discord_ParseTimestamp(JsonGetString(obj, "last_pin_timestamp"));
	client->onevent.channel_pins_update(client, guild_id, channel_id, last_pin_timestamp);
}

static void Discord_EventHandlerThreadCreate(Discord::Client *client, const Json &data) {
	Discord::Channel thread;
	Json_Object obj = JsonGetObject(data);
	Discord_Deserialize(obj, &thread);
	bool newly_created = JsonGetBool(obj, "newly_created");
	client->onevent.thread_create(client, thread, newly_created);
}

static void Discord_EventHandlerThreadUpdate(Discord::Client *client, const Json &data) {
	Discord::Channel thread;
	Json_Object obj = JsonGetObject(data);
	Discord_Deserialize(obj, &thread);
	client->onevent.thread_update(client, thread);
}

static void Discord_EventHandlerThreadDelete(Discord::Client *client, const Json &data) {
	Json_Object obj  = JsonGetObject(data);
	Discord::Snowflake id        = Discord_ParseId(JsonGetString(obj, "id"));
	Discord::Snowflake guild_id  = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Snowflake parent_id = Discord_ParseId(JsonGetString(obj, "parent_id"));
	Discord::ChannelType type    = (Discord::ChannelType)JsonGetInt(obj, "type");
	client->onevent.thread_delete(client, id, guild_id, parent_id, type);
}

static void Discord_EventHandlerThreadListSync(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Json_Array jsonchannel_ids = JsonGetArray(obj, "channel_ids");
	Array<Discord::Snowflake> channel_ids;
	channel_ids.Resize(jsonchannel_ids.count);
	for (ptrdiff_t index = 0; index < channel_ids.count; ++index) {
		channel_ids[index] = Discord_ParseId(JsonGetString(jsonchannel_ids[index]));
	}

	Json_Array jsonthreads = JsonGetArray(obj, "threads");
	Array<Discord::Channel> threads;
	threads.Resize(jsonthreads.count);
	for (ptrdiff_t index = 0; index < threads.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonthreads[index]), &threads[index]);
	}

	Json_Array jsonmembers = JsonGetArray(obj, "members");
	Array<Discord::ThreadMember> members;
	members.Resize(jsonmembers.count);
	for (ptrdiff_t index = 0; index < members.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonmembers[index]), &threads[index]);
	}

	client->onevent.thread_list_sync(client, guild_id, channel_ids, threads, members);
}

static void Discord_EventHandlerThreadMemberUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::ThreadMember member;
	Discord_Deserialize(obj, &member);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent.thread_member_update(client, guild_id, member);
}

static void Discord_EventHandlerThreadMembersUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj                 = JsonGetObject(data);
	Discord::Snowflake id           = Discord_ParseId(JsonGetString(obj, "id"));
	Discord::Snowflake guild_id     = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Snowflake member_count = JsonGetInt(obj, "member_count");

	Json_Array jsonadded_members = JsonGetArray(obj, "added_members");
	Array<Discord::ThreadMember> added_members;
	added_members.Resize(jsonadded_members.count);
	for (ptrdiff_t index = 0; index < added_members.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonadded_members[index]), &added_members[index]);
	}

	Json_Array jsonremoved_member_ids = JsonGetArray(obj, "removed_member_ids");
	Array<Discord::Snowflake> removed_member_ids;
	removed_member_ids.Resize(removed_member_ids.count);
	for (ptrdiff_t index = 0; index < removed_member_ids.count; ++index) {
		removed_member_ids[index] = Discord_ParseId(JsonGetString(jsonremoved_member_ids[index]));
	}

	client->onevent.thread_members_update(client, id, guild_id, member_count, added_members, removed_member_ids);
}

static void Discord_EventHandlerGuildCreate(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);

	Discord::Guild guild;
	Discord_Deserialize(obj, &guild);

	Discord::GuildInfo info;
	info.joined_at    = Discord_ParseTimestamp(JsonGetString(obj, "joined_at"));
	info.large        = JsonGetBool(obj, "large");
	info.unavailable  = JsonGetBool(obj, "unavailable");
	info.member_count = JsonGetInt(obj, "member_count");

	Json_Array jsonvoice_states = JsonGetArray(obj, "voice_states");
	Array<Discord::VoiceState> voice_states;
	voice_states.Resize(jsonvoice_states.count);
	for (ptrdiff_t index = 0; index < voice_states.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonvoice_states[index]), &voice_states[index]);
	}
	info.voice_states = voice_states;

	Json_Array jsonmembers = JsonGetArray(obj, "members");
	Array<Discord::GuildMember> members;
	members.Resize(jsonmembers.count);
	for (ptrdiff_t index = 0; index < members.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonmembers[index]), &members[index]);
	}
	info.members = members;

	Json_Array jsonchannels = JsonGetArray(obj, "channels");
	Array<Discord::Channel> channels;
	channels.Resize(jsonchannels.count);
	for (ptrdiff_t index = 0; index < channels.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonchannels[index]), &channels[index]);
	}
	info.channels = channels;
	
	Json_Array jsonthreads = JsonGetArray(obj, "threads");
	Array<Discord::Channel> threads;
	threads.Resize(jsonthreads.count);
	for (ptrdiff_t index = 0; index < threads.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonthreads[index]), &threads[index]);
	}
	info.threads = threads;

	Json_Array jsonpresences = JsonGetArray(obj, "presences");
	Array<Discord::Presence> presences;
	presences.Resize(jsonpresences.count);
	for (ptrdiff_t index = 0; index < presences.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonpresences[index]), &presences[index]);
	}
	info.presences = presences;

	Json_Array jsonstage_instances = JsonGetArray(obj, "stage_instances");
	Array<Discord::StageInstance> stage_instances;
	stage_instances.Resize(jsonstage_instances.count);
	for (ptrdiff_t index = 0; index < stage_instances.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonstage_instances[index]), &stage_instances[index]);
	}
	info.stage_instances = stage_instances;
	
	Json_Array jsonguild_scheduled_events = JsonGetArray(obj, "guild_scheduled_events");
	Array<Discord::GuildScheduledEvent> guild_scheduled_events;
	guild_scheduled_events.Resize(jsonguild_scheduled_events.count);
	for (ptrdiff_t index = 0; index < guild_scheduled_events.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonguild_scheduled_events[index]), &guild_scheduled_events[index]);
	}
	info.guild_scheduled_events = guild_scheduled_events;

	client->onevent.guild_create(client, guild, info);
}

static void Discord_EventHandlerGuildUpdate(Discord::Client *client, const Json &data) {
	Discord::Guild guild;
	Discord_Deserialize(JsonGetObject(data), &guild);
	client->onevent.guild_update(client, guild);
}

static void Discord_EventHandlerGuildDelete(Discord::Client *client, const Json &data) {
	Discord::UnavailableGuild guild;
	Discord_Deserialize(JsonGetObject(data), &guild);
	client->onevent.guild_delete(client, guild);
}

static void Discord_EventHandlerGuildBanAdd(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::User user;
	Discord_Deserialize(JsonGetObject(obj, "user"), &user);
	client->onevent.guild_ban_add(client, guild_id, user);
}

static void Discord_EventHandlerGuildBanRemove(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::User user;
	Discord_Deserialize(JsonGetObject(obj, "user"), &user);
	client->onevent.guild_ban_remove(client, guild_id, user);
}

static void Discord_EventHandlerGuildEmojisUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj             = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	
	Json_Array jsonemojis = JsonGetArray(obj, "emojis");
	Array<Discord::Emoji> emojis_update;
	emojis_update.Resize(jsonemojis.count);
	for (ptrdiff_t index = 0; index < emojis_update.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonemojis[index]), &emojis_update[index]);
	}

	client->onevent.guild_emojis_update(client, guild_id, emojis_update);
}

static void Discord_EventHandlerGuildStickersUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj             = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Json_Array jsonstickers = JsonGetArray(obj, "stickers");
	Array<Discord::Sticker> stickers;
	stickers.Resize(jsonstickers.count);
	for (ptrdiff_t index = 0; index < stickers.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonstickers[index]), &stickers[index]);
	}

	client->onevent.guild_stickers_update(client, guild_id, stickers);
}

static void Discord_EventHandlerGuildIntegrationsUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj             = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent.guild_integrations_update(client, guild_id);
}

static void Discord_EventHandlerGuildMemberAdd(Discord::Client *client, const Json &data) {
	Discord::GuildMember member;
	Json_Object obj             = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord_Deserialize(obj, &member);
	client->onevent.guild_member_add(client, guild_id, member);
}

static void Discord_EventHandlerGuildMemberRemove(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::User user;
	Discord_Deserialize(JsonGetObject(obj, "user"), &user);
	client->onevent.guild_member_remove(client, guild_id, user);
}

static void Discord_EventHandlerGuildMemberUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Discord::GuildMemberUpdate member;

	Json_Array jsonroles = JsonGetArray(obj, "roles");
	Array<Discord::Snowflake> roles;
	roles.Resize(jsonroles.count);
	for (ptrdiff_t index = 0; index < roles.count; ++index) {
		roles[index] = Discord_ParseId(JsonGetString(jsonroles[index]));
	}
	member.roles = roles;

	Discord_Deserialize(JsonGetObject(obj, "user"), &member.user);

	member.nick                         = JsonGetString(obj, "nick");
	member.avatar                       = JsonGetString(obj, "avatar");
	member.joined_at                    = Discord_ParseTimestamp(JsonGetString(obj, "joined_at"));
	member.premium_since                = Discord_ParseTimestamp(JsonGetString(obj, "premium_since"));
	member.deaf                         = JsonGetBool(obj, "deaf");
	member.mute                         = JsonGetBool(obj, "mute");
	member.pending                      = JsonGetBool(obj, "pending");
	member.communication_disabled_until = Discord_ParseTimestamp(JsonGetString(obj, "communication_disabled_until"));

	client->onevent.guild_member_update(client, guild_id, member);
}

static void Discord_EventHandlerGuildMembersChunk(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));

	Discord::GuildMembersChunk chunk;
	Json_Array jsonmembers = JsonGetArray(obj, "members");
	Array<Discord::GuildMember> members;
	members.Resize(jsonmembers.count);
	for (ptrdiff_t index = 0; index < members.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonmembers[index]), &members[index]);
	}
	chunk.members = members;

	chunk.chunk_index = JsonGetInt(obj, "chunk_index");
	chunk.chunk_count = JsonGetInt(obj, "chunk_count");

	Json_Array jsonnot_found = JsonGetArray(obj, "not_found");
	Array<Discord::Snowflake> not_found;
	not_found.Resize(jsonnot_found.count);
	for (ptrdiff_t index = 0; index < not_found.count; ++index) {
		not_found[index] = Discord_ParseId(JsonGetString(jsonnot_found[index]));
	}
	chunk.not_found = not_found;

	Json_Array jsonpresences = JsonGetArray(obj, "presences");
	Array<Discord::Presence> presences;
	presences.Resize(jsonpresences.count);
	for (ptrdiff_t index = 0; index < presences.count; ++index) {
		Discord_Deserialize(JsonGetObject(jsonpresences[index]), &presences[index]);
	}
	chunk.presences = presences;

	chunk.nonce = JsonGetString(obj, "nonce");

	client->onevent.guild_members_chunk(client, guild_id, chunk);
}

static void Discord_EventHandlerGuildRoleCreate(Discord::Client *client, const Json &data) {
	Json_Object obj             = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Role role;
	Discord_Deserialize(JsonGetObject(obj, "role"), &role);
	client->onevent.guild_role_create(client, guild_id, role);
}

static void Discord_EventHandlerGuildRoleUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj             = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Role role;
	Discord_Deserialize(JsonGetObject(obj, "role"), &role);
	client->onevent.guild_role_update(client, guild_id, role);
}

static void Discord_EventHandlerGuildRoleDelete(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Snowflake role_id    = Discord_ParseId(JsonGetString(obj, "role_id"));
	client->onevent.guild_role_delete(client, guild_id, role_id);
}

static void Discord_EventHandlerGuildScheduledEventCreate(Discord::Client *client, const Json &data) {
	Discord::GuildScheduledEvent scheduled_event;
	Discord_Deserialize(JsonGetObject(data), &scheduled_event);
	client->onevent.guild_scheduled_event_create(client, scheduled_event);
}

static void Discord_EventHandlerGuildScheduledEventUpdate(Discord::Client *client, const Json &data) {
	Discord::GuildScheduledEvent scheduled_event;
	Discord_Deserialize(JsonGetObject(data), &scheduled_event);
	client->onevent.guild_scheduled_event_update(client, scheduled_event);
}

static void Discord_EventHandlerGuildScheduledEventDelete(Discord::Client *client, const Json &data) {
	Discord::GuildScheduledEvent scheduled_event;
	Discord_Deserialize(JsonGetObject(data), &scheduled_event);
	client->onevent.guild_scheduled_event_delete(client, scheduled_event);
}

static void Discord_EventHandlerGuildScheduledEventUserAdd(Discord::Client *client, const Json &data) {
	Json_Object obj                             = JsonGetObject(data);
	Discord::Snowflake guild_scheduled_event_id = Discord_ParseId(JsonGetString(obj, "guild_scheduled_event_id"));
	Discord::Snowflake user_id                  = Discord_ParseId(JsonGetString(obj, "user_id"));
	Discord::Snowflake guild_id                 = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent.guild_scheduled_event_user_add(client, guild_scheduled_event_id, user_id, guild_id);
}

static void Discord_EventHandlerGuildScheduledEventUserRemove(Discord::Client *client, const Json &data) {
	Json_Object obj                             = JsonGetObject(data);
	Discord::Snowflake guild_scheduled_event_id = Discord_ParseId(JsonGetString(obj, "guild_scheduled_event_id"));
	Discord::Snowflake user_id                  = Discord_ParseId(JsonGetString(obj, "user_id"));
	Discord::Snowflake guild_id                 = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent.guild_scheduled_event_user_remove(client, guild_scheduled_event_id, user_id, guild_id);
}

static void Discord_EventHandlerIntegrationCreate(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Integration integration;
	Discord_Deserialize(obj, &integration);
	client->onevent.integration_create(client, guild_id, integration);
}

static void Discord_EventHandlerIntegrationUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj = JsonGetObject(data);
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Integration integration;
	Discord_Deserialize(obj, &integration);
	client->onevent.integration_update(client, guild_id, integration);
}

static void Discord_EventHandlerIntegrationDelete(Discord::Client *client, const Json &data) {
	Json_Object obj                   = JsonGetObject(data);
	Discord::Snowflake id             = Discord_ParseId(JsonGetString(obj, "id"));
	Discord::Snowflake guild_id       = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Snowflake application_id = Discord_ParseId(JsonGetString(obj, "application_id"));
	client->onevent.integration_delete(client, id, guild_id, application_id);
}

static void Discord_EventHandlerInteractionCreate(Discord::Client *client, const Json &data) {
	Discord::Interaction interation;
	Discord_Deserialize(JsonGetObject(data), &interation);
	client->onevent.interaction_create(client, interation);
}

static void Discord_EventHandlerInviteCreate(Discord::Client *client, const Json &data) {
	Discord::InviteInfo info;
	Json_Object obj = JsonGetObject(data);
	info.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	info.code       = JsonGetString(obj, "code");
	info.created_at = Discord_ParseTimestamp(JsonGetString(obj, "created_at"));
	info.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));

	const Json *inviter = obj.Find("inviter");
	if (inviter) {
		info.inviter = new Discord::User;
		if (info.inviter) {
			Discord_Deserialize(JsonGetObject(*inviter), info.inviter);
		}
	}

	info.max_age     = JsonGetInt(obj, "max_age");
	info.max_uses    = JsonGetInt(obj, "max_uses");
	info.target_type = (Discord::InviteTargetType)JsonGetInt(obj, "target_type");

	const Json *target_user = obj.Find("target_user");
	if (target_user) {
		info.target_user = new Discord::User;
		if (info.target_user) {
			Discord_Deserialize(JsonGetObject(*target_user), info.target_user);
		}
	}

	const Json *target_application = obj.Find("target_application");
	if (target_application) {
		info.target_application = new Discord::Application;
		if (info.target_application) {
			Discord_Deserialize(JsonGetObject(*target_application), info.target_application);
		}
	}

	info.temporary = JsonGetBool(obj, "temporary");
	info.uses      = JsonGetInt(obj, "uses");

	client->onevent.invite_create(client, info);
}

static void Discord_EventHandlerInviteDelete(Discord::Client *client, const Json &data) {
	Json_Object obj               = JsonGetObject(data);
	Discord::Snowflake channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	String code                   = JsonGetString(obj, "code");
	client->onevent.invite_delete(client, channel_id, guild_id, code);
}

static void Discord_EventHandlerMessageCreate(Discord::Client *client, const Json &data) {
	Discord::Message message;
	Discord_Deserialize(JsonGetObject(data), &message);
	client->onevent.message_create(client, message);
}

static void Discord_EventHandlerMessageUpdate(Discord::Client *client, const Json &data) {
	Discord::Message message;
	Discord_Deserialize(JsonGetObject(data), &message);
	client->onevent.message_update(client, message);
}

static void Discord_EventHandlerMessageDelete(Discord::Client *client, const Json &data) {
	Json_Object obj               = JsonGetObject(data);
	Discord::Snowflake id         = Discord_ParseId(JsonGetString(obj, "id"));
	Discord::Snowflake channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent.message_delete(client, id, channel_id, guild_id);
}

static void Discord_EventHandlerMessageDeleteBulk(Discord::Client *client, const Json &data) {
	Json_Object obj    = JsonGetObject(data);

	Json_Array jsonids = JsonGetArray(obj, "ids");
	Array<Discord::Snowflake> ids;
	ids.Resize(jsonids.count);
	for (ptrdiff_t index = 0; index < ids.count; ++index) {
		ids[index] = Discord_ParseId(JsonGetString(jsonids[index]));
	}
	
	Discord::Snowflake channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent.message_delete_bulk(client, ids, channel_id, guild_id);
}

static void Discord_EventHandlerMessageReactionAdd(Discord::Client *client, const Json &data) {
	Discord::MessageReactionInfo reaction;
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
	client->onevent.message_reaction_add(client, reaction);
}

static void Discord_EventHandlerMessageReactionRemove(Discord::Client *client, const Json &data) {
	Json_Object obj               = JsonGetObject(data);
	Discord::Snowflake user_id    = Discord_ParseId(JsonGetString(obj, "user_id"));
	Discord::Snowflake channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	Discord::Snowflake message_id = Discord_ParseId(JsonGetString(obj, "message_id"));
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Emoji emoji;
	Discord_Deserialize(JsonGetObject(obj, "emoji"), &emoji);
	client->onevent.message_reaction_remove(client, user_id, channel_id, message_id, guild_id, emoji);
}

static void Discord_EventHandlerMessageReactionRemoveAll(Discord::Client *client, const Json &data) {
	Json_Object obj               = JsonGetObject(data);
	Discord::Snowflake channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	Discord::Snowflake message_id = Discord_ParseId(JsonGetString(obj, "message_id"));
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	client->onevent.message_reaction_remove_all(client, channel_id, message_id, guild_id);
}

static void Discord_EventHandlerMessageReactionRemoveEmoji(Discord::Client *client, const Json &data) {
	Json_Object obj     = JsonGetObject(data);
	Discord::Snowflake channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Snowflake message_id = Discord_ParseId(JsonGetString(obj, "message_id"));
	Discord::Emoji emoji;
	Discord_Deserialize(JsonGetObject(obj, "emoji"), &emoji);
	client->onevent.message_reaction_remove_emoji(client, channel_id, guild_id, message_id, emoji);
}

static void Discord_EventHandlerPresenceUpdate(Discord::Client *client, const Json &data) {
	Discord::Presence presence;
	Discord_Deserialize(JsonGetObject(data), &presence);
	client->onevent.presence_update(client, presence);
}

static void Discord_EventHandlerStageInstanceCreate(Discord::Client *client, const Json &data) {
	Discord::StageInstance stage;
	Discord_Deserialize(JsonGetObject(data), &stage);
	client->onevent.stage_instance_create(client, stage);
}

static void Discord_EventHandlerStageInstanceDelete(Discord::Client *client, const Json &data) {
	Discord::StageInstance stage;
	Discord_Deserialize(JsonGetObject(data), &stage);
	client->onevent.stage_instance_delete(client, stage);
}

static void Discord_EventHandlerStageInstanceUpdate(Discord::Client *client, const Json &data) {
	Discord::StageInstance stage;
	Discord_Deserialize(JsonGetObject(data), &stage);
	client->onevent.stage_instance_update(client, stage);
}

static void Discord_EventHandlerTypingStartEvent(Discord::Client *client, const Json &data) {
	Discord::TypingStartInfo typing;
	Json_Object obj   = JsonGetObject(data);
	typing.channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	typing.guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	typing.user_id    = Discord_ParseId(JsonGetString(obj, "user_id"));
	typing.timestamp  = JsonGetInt(obj, "timestamp");

	const Json *member = obj.Find("member");
	if (member) {
		typing.member = new Discord::GuildMember;
		if (typing.member) {
			Discord_Deserialize(JsonGetObject(*member), typing.member);
		}
	}

	client->onevent.typing_start(client, typing);
}

static void Discord_EventHandlerUserUpdate(Discord::Client *client, const Json &data) {
	Discord::User user;
	Discord_Deserialize(JsonGetObject(data), &user);
	client->onevent.user_update(client, user);
}

static void Discord_EventHandlerVoiceStateUpdate(Discord::Client *client, const Json &data) {
	Discord::VoiceState voice;
	Discord_Deserialize(JsonGetObject(data), &voice);
	client->onevent.voice_state_update(client, voice);
}

static void Discord_EventHandlerVoiceServerUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj             = JsonGetObject(data);
	String token                = JsonGetString(obj, "token");
	Discord::Snowflake guild_id = Discord_ParseId(JsonGetString(obj, "guild_id"));
	String endpoint             = JsonGetString(obj, "endpoint");
	client->onevent.voice_server_update(client, token, guild_id, endpoint);
}

static void Discord_EventHandlerWebhooksUpdate(Discord::Client *client, const Json &data) {
	Json_Object obj               = JsonGetObject(data);
	Discord::Snowflake guild_id   = Discord_ParseId(JsonGetString(obj, "guild_id"));
	Discord::Snowflake channel_id = Discord_ParseId(JsonGetString(obj, "channel_id"));
	client->onevent.webhooks_update(client, guild_id, channel_id);
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
	Discord_EventHandlerStageInstanceCreate, Discord_EventHandlerStageInstanceDelete, Discord_EventHandlerStageInstanceUpdate,
	Discord_EventHandlerTypingStartEvent,Discord_EventHandlerUserUpdate, Discord_EventHandlerVoiceStateUpdate,
	Discord_EventHandlerVoiceServerUpdate, Discord_EventHandlerWebhooksUpdate,
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

static bool Discord_GatewayCloseReconnect(int code) {
	if (code >= 4000 && code <= 4009 && code != 4006)
		return true;
	return false;
}

static void Discord_HandleWebsocketEvent(Discord::Client *client, const Websocket_Event &event) {
	if (event.type == WEBSOCKET_EVENT_TEXT) {
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
				Discord::HearbeatCommand(client);
				TraceEx("Discord", "Heartbeat (%d)", client->heartbeat.count);
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
				// If session_id is present, then there's a possibility that the connection can be resumed
				if (strlen((char *)client->session_id)) {
					TraceEx("Discord", "Resuming session: %s", client->session_id);
					Discord::ResumeCommand(client);
				} else {
					Discord::IdentifyCommand(client);
				}
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
		return;
	}

	if (event.type == WEBSOCKET_EVENT_CLOSE) {
		int code       = Websocket_EventCloseCode(event);
		String message = Websocket_EventCloseMessage(event);

		if (code == WEBSOCKET_CLOSE_GOING_AWAY || code == WEBSOCKET_CLOSE_NORMAL)
			LogInfoEx("Discord", "Shutting down...");
		else if (message.length)
			LogErrorEx("Discord", "Shutting down (%d): " StrFmt, code, message);
		else
			LogErrorEx("Discord", "Abnormal shut down (%d)", code);

		client->login = Discord_GatewayCloseReconnect(code);
	}
}
