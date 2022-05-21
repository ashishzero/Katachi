#include "Discord.h"
#include "Kr/KrString.h"
#include "Base64.h"

#include <stdio.h>

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//


void LogProcedure(void *context, Log_Level level, const char *source, const char *fmt, va_list args) {
	FILE *fp = level == LOG_LEVEL_INFO ? stdout : stderr;
	fprintf(fp, "[%s] ", source);
	vfprintf(fp, fmt, args);
	fprintf(fp, "\n");
}

static volatile bool Logout = false;
static String        ImageFile = "";

void OnTick(Discord::Client *client) {
	if (Logout) {
		Discord::Logout(client);
	}
}

void OnReaction(Discord::Client *client, const Discord::MessageReactionInfo &reaction) {
	String poop = "%F0%9F%92%A9";
	if (reaction.emoji.name == u8"💩") {
		Discord::Message *msg = Discord::GetChannelMessage(client, reaction.channel_id, reaction.message_id);
		if (msg && StrFind(msg->content, "zero") >= 0) {
			Discord::DeleteUserReaction(client, reaction.channel_id, reaction.message_id, poop, reaction.user_id);
		}
	} else {
		Trace("Emoji Name: " StrFmt, StrArg(reaction.emoji.name));
	}
}

void OnMessage(Discord::Client *client, const Discord::Message &message) {
	if (message.content == "info") {
		Discord::Channel *channel = Discord::GetChannel(client, message.channel_id);
		if (channel) {
			Discord::MessagePost reply;
			reply.content = channel->name;
			Discord::CreateMessage(client, message.channel_id, reply);
		}
	} else if (StrStartsWith(message.content, "rename ")) {
		String name = StrTrim(SubStr(message.content, 7));
		if (name.length) {
			Discord::ChannelPatch patch;
			patch.name = name;
			Discord::ModifyChannel(client, message.channel_id, patch);
		}
	} else if (message.content == "deleteme") {
		Discord::DeleteChannel(client, message.channel_id);
	} else if (message.content == "delete-bulk") {
		auto msgs = Discord::GetChannelMessages(client, message.channel_id);
		Discord::MessagePost post;
		post.content = FmtStr(ThreadContext.allocator, "Deleting %d messages...", (int)msgs.count);
		auto sent = Discord::CreateMessage(client, message.channel_id, post);
		Trace("Deleting %d messages...", (int)msgs.count);
		Array<Discord::Snowflake> ids;
		ids.Resize(msgs.count);
		for (ptrdiff_t index = 0; index < ids.count; ++index) {
			ids[index] = msgs[index].id;
		}
		if (Discord::BulkDeleteMessages(client, message.channel_id, ids) && sent) {
			Discord::MessagePatch patch;
			patch.content = new String(FmtStr(ThreadContext.allocator, "Deleted %d messages", (int)msgs.count));
			Discord::EditMessage(client, message.channel_id, sent->id, patch);
		}
	} else if (message.content == "ping") {
		Discord::Embed embed;
		embed.description = "pong";
		embed.title = "ping";
		embed.color = 0x00ffff;

		Discord::MessagePost reply;
		reply.embeds.Add(embed);
		Discord::CreateMessage(client, message.channel_id, reply);
	} else if (message.content == "me") {
		Discord::FileAttachment file;
		file.content      = ImageFile;
		file.content_type = "image/png";
		file.filename     = "me.png";
		file.description  = "This is some desc";

		Discord::Embed embed;
		embed.title          = "Embed";
		embed.description    = "This is embedded msg";
		embed.thumbnail      = new Discord::EmbedThumbnail;
		embed.thumbnail->url = "attachment://me.png";
		embed.image          = new Discord::EmbedImage;
		embed.image->url     = "attachment://me.png";

		Discord::MessagePost reply;
		reply.attachments.Add(file);
		reply.content = "Image";
		reply.embeds.Add(embed);
		Discord::CreateMessage(client, message.channel_id, reply);
	} else if (message.content == "editmsg") {
		Discord::MessagePost reply;
		reply.content = "This is original message";
		Discord::Message *msg = Discord::CreateMessage(client, message.channel_id, reply);
		if (msg) {
			Discord::MessagePatch edited;
			edited.content = new String("The message is edited now");
			Discord::EditMessage(client, msg->channel_id, msg->id, edited);
		}
	} else if (message.content == "editperms") {
		Discord::Channel *channel = Discord::GetChannel(client, message.channel_id);
		if (channel) {
			if (channel->permission_overwrites.count) {
				auto perms = channel->permission_overwrites[0];
				perms.deny = ToggleFlag(perms.deny, Discord::PermissionBit::SEND_MESSAGES);
				Discord::EditChannelPermissions(client, message.channel_id, perms.id, perms.allow, perms.deny, perms.type);
				Trace("Edited Permissions: %zu, %zu, %d", perms.allow, perms.deny, perms.type);
			}
		}
	} else if (StrFind(message.content, "one") >= 0) {
		Discord::DeleteMessage(client, message.channel_id, message.id);
	} else {
		if (StrFind(message.content, "zero") >= 0) {
			String heart  = "%F0%9F%92%96";
			String potato = "%F0%9F%A5%94";
			Discord::CreateReaction(client, message.channel_id, message.id, potato);
			Discord::CreateReaction(client, message.channel_id, message.id, heart);
			Discord::DeleteReaction(client, message.channel_id, message.id, potato);
		}

		Discord::Channel *channel = Discord::GetChannel(client, message.channel_id);
		if (channel && channel->type == Discord::ChannelType::GUILD_NEWS) {
			Discord::CrossPost(client, message.channel_id, message.id);
		}
	}
}

static void InterruptHandler(int signo) {
	Logout = true;
}

#include <signal.h>

static String ReadEntireFile(const char *filename) {
	FILE *f = fopen(filename, "rb");

	if (!f)
		return String();

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *string = (char *)MemoryAllocate(fsize + 1);
	fread(string, fsize, 1, f);
	fclose(f);

	string[fsize] = 0;
	return String(string, fsize);
}

int main(int argc, char **argv) {
	InitThreadContext(0);
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	ImageFile = ReadEntireFile("SampleImage.png");
	Assert(ImageFile.length);

	signal(SIGINT, InterruptHandler);

	Discord::Initialize();

	String token = String(argv[1], strlen(argv[1]));

	int intents = 0;
	intents |= Discord::Intent::GUILDS;
	intents |= Discord::Intent::GUILD_MEMBERS;
	intents |= Discord::Intent::GUILD_MESSAGES;
	intents |= Discord::Intent::GUILD_MESSAGE_REACTIONS;
	intents |= Discord::Intent::GUILD_VOICE_STATES;
	intents |= Discord::Intent::DIRECT_MESSAGES;
	intents |= Discord::Intent::DIRECT_MESSAGE_REACTIONS;

	Discord::PresenceUpdate presence;
	presence.status = Discord::StatusType::DO_NOT_DISTURB;
	auto activity   = presence.activities.Add();
	activity->name  = "Twitch";
	activity->url   = "https://www.twitch.tv/ashishzero";
	activity->type  = Discord::ActivityType::STREAMING;

	Discord::EventHandler events;
	events.tick           = OnTick;
	events.message_create = OnMessage;
	events.message_reaction_add = OnReaction;

	Discord::LoginSharded(token, intents, events, &presence);

	return 0;
}
