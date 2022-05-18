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

#if 1

#include "Network.h"
#include "Http.h"
#include "Kr/KrThread.h"
#include "Kr/KrAtomic.h"

typedef void (*Http_Response_Proc)(const Http_Response &res, void *);
typedef void (*Http_Error_Proc)(void *);

struct Http_Service_Handler {
	Http_Response_Proc response;
	Http_Error_Proc    error;
	void *             context;
};

struct Http_Service_Request {
	Http_Service_Request *next;
	Http_Connection       type;
	String                hostname;
	String                port;
	String                header;
	Http_Reader           reader;
	Http_Writer           writer;
	Http_Service_Handler  handler;
	ptrdiff_t             bufflen;
	uint8_t               buffer[HTTP_MAX_HEADER_SIZE];
};

struct Http_Service {
	volatile bool         active;
	Semaphore *           read;
	Semaphore *           aval;
	Atomic_Guard          rguard;
	Atomic_Guard          wguard;
	Atomic_Guard          memguard;
	Http_Service_Request *head;
	Http_Service_Request *tail;
	Http_Service_Request *free;
	Memory_Arena *        arena;
};

static Http_Service_Request *HttpService_AllocateRequest(Http_Service *service) {
	Http_Service_Request *req = nullptr;
	SpinLock(&service->memguard);
	if (service->free) {
		req = service->free;
		service->free = req->next;
	} else {
		req = PushType(service->arena, Http_Service_Request);
		if (!req) {
			
		}
	}
	SpinUnlock(&service->memguard);
	return req;
}

int HttpService_ThreadProc(void *arg) {
	Http_Service *service = (Http_Service *)arg;

	while (service->active) {
		int wait = Semaphore_Wait(service->read, 5000);
		if (wait > 0) {
			Http_Service_Request *req;
			Http *http;

			if (!Http_SendRequest(http, req->header, req->reader)) {
				req->handler.error(req->handler.context);
				continue;
			}

			Http_Response res;
			if (!Http_ReceiveResponse(http, &res, req->writer)) {
				req->handler.error(req->handler.context);
				continue;
			}

			req->handler.response(res, req->handler.context);
		}
	}

	return 0;
}

#endif

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
