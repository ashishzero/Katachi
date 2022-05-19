#include "Discord.h"
#include "Kr/KrString.h"

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

void TestEventHandler(Discord::Client *client, const Discord::Event *event) {
	if (event->type == Discord::EventType::TICK) {
		if (Logout) {
			Discord::Logout(client);
		}
		return;
	}

	if (event->type == Discord::EventType::READY) {
		auto ready = (Discord::ReadyEvent *)event;
		Trace("Bot online " StrFmt "#" StrFmt, StrArg(ready->user.username), StrArg(ready->user.discriminator));
		Trace("Discord Gateway Version : %d", ready->v);
		Discord::Shard shard = Discord::GetShard(client);
		Trace("Shard Tick: %d, %d", shard.id, shard.count);
		return;
	}

	if (event->type == Discord::EventType::GUILD_CREATE) {
		auto guild = (Discord::GuildCreateEvent *)event;
		Trace("Joined guild: " StrFmt, StrArg(guild->guild.name));
		return;
	}

	if (event->type == Discord::EventType::MESSAGE_CREATE) {
		Discord::Shard shard = Discord::GetShard(client);
		Trace("Shard Tick: %d, %d", shard.id, shard.count);
		auto msg = (Discord::MessageCreateEvent *)event;
		
		if (msg->message.content == "info") {
			Discord::Channel *channel = Discord::GetChannel(client, msg->message.channel_id);
			if (channel) {
				Trace("Channel name" StrFmt, StrArg(channel->name));
			} else {
				Trace("Channle not found!");
			}
		} else if (StrStartsWith(msg->message.content, "rename ")) {
			String name = StrTrim(SubStr(msg->message.content, 7));
			if (name.length) {
				Discord::ChannelPatch patch;
				Discord::PatchName(&patch, name);
				Discord::ModifyChannel(client, msg->message.channel_id, patch);
			}
		} else if (msg->message.content == "deleteme") {
			Discord::DeleteChannel(client, msg->message.channel_id);
		}

		return;
	}
}

static void InterruptHandler(int signo) {
	Logout = true;
}

#include <signal.h>

int main(int argc, char **argv) {
	InitThreadContext(0);
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

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

	Discord::LoginSharded(token, intents, TestEventHandler, &presence);

	return 0;
}
