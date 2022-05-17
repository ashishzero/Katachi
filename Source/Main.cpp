#include "Discord.h"

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
		Trace("Message sent: \"" StrFmt "\" by " StrFmt, StrArg(msg->message.content), StrArg(msg->message.author.username));
		return;
	}
}

#include "Network.h"
#include "Http.h"
#include "Kr/KrThread.h"
#include "Kr/KrAtomic.h"

struct Http_Service_Connection {
	Http *          http;
	Http_Connection type;
	String          hostname;
	String          port;
	uint8_t         buffer[NET_MAX_CANON_NAME];
};

struct Http_Service_Request {
	Http_Service_Request *next;
	Http_Service_Request *prev;
	Http_Connection       type;
	String                hostname;
	String                port;
	String                header;
	uint8_t               buffer[HTTP_MAX_HEADER_SIZE];
	Buffer                body;
	Memory_Arena *        arena;
};

struct Http_Service_Request_Queue {
	Http_Service_Request *head;
	Http_Service_Request *tail;
	Http_Service_Request *free;
	Atomic_Guard          guard;
};

struct Http_Service {
	volatile bool              running = false;
	ptrdiff_t                  p2nconnection;
	Http_Service_Connection *  connections;
	Http_Service_Request_Queue queue;
	Semaphore *                read;
	Semaphore *                write;
	Memory_Arena *             arena = nullptr;
};

Http_Service_Request *HttpService_QueuePop(Http_Service_Request_Queue *q) {
	SpinLock(&q->guard);
	Http_Service_Request *req = q->head;
	q->head = q->head->next;
	req->next = nullptr;
	SpinUnlock(&q->guard);
	return req;
}

int HttpService_ThreadProc(void *arg) {
	Http_Service *service = (Http_Service *)arg;

	while (service->running) {
		int wait = Semaphore_Wait(service->read, 5000);
		if (wait > 0) {

		}
	}

	return 0;
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
