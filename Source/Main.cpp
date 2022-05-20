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

void OnTick(Discord::Client *client) {
	if (Logout) {
		Discord::Logout(client);
	}
}

void OnMessage(Discord::Client *client, const Discord::Message &message) {
	if (message.content == "info") {
		Discord::Channel *channel = Discord::GetChannel(client, message.channel_id);
		if (channel) {
			Trace("Channel name" StrFmt, StrArg(channel->name));
		} else {
			Trace("Channle not found!");
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
	} else if (message.content == "getmsgs") {
		auto msgs = Discord::GetChannelMessages(client, message.channel_id);
		Trace("Get Msgs Count: %d", (int)msgs.count);
		for (auto &m : msgs) {
			Trace(StrFmt, StrArg(m.content));
		}
	} else if (message.content == "getmsg") {
		auto current = Discord::GetChannelMessage(client, message.channel_id, message.id);
		Trace(StrFmt, StrArg(current->content));
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

	Discord::EventHandler events;
	events.tick           = OnTick;
	events.message_create = OnMessage;

	Discord::LoginSharded(token, intents, events, &presence);

	return 0;
}
