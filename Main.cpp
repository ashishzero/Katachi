#include "KrBasic.h"
#include "StringBuilder.h"
#include "Network.h"
#include "Discord.h"

#include <stdio.h>

#include "Json.h"

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//

static const String NoCheckinJSON = u8R"foo(
{
	"reactions": [
	{
		"count": 1,
			"me": false,
			"emoji": {
			"id": null,
			"name": "🔥"
		}
	}
	],
		"attachments": [],
		"tts": false,
		"embeds": [],
		"timestamp": "2017-07-11T17:27:07.299000+00:00",
		"mention_everyone": false,
		"id": "334385199974967042",
		"pinned": false,
		"edited_timestamp": null,
		"author": {
		"username": "Mason",
			"discriminator": "9999",
			"id": "53908099506183680",
			"avatar": "a_bab14f271d565501444b2ca3be944b25"
	},
		"mention_roles": [],
		"mention_channels": [
	{
		"id": "278325129692446722",
			"guild_id": "278325129692446720",
			"name": "big-news",
			"type": 5
	}
	],
		"content": "Big news! In this <#278325129692446722> channel!",
		"channel_id": "290926798999357250",
		"mentions": [],
		"type": 0,
		"flags": 2,
		"message_reference": {
		"channel_id": "278325129692446722",
		"guild_id": "278325129692446720",
		"message_id": "306588351130107906"
	}
}
)foo";

void JsonPrint(const Json *json, int depth);

void JsonPrintIndent(int depth) {
	for (int i = 0; i < depth * 3; ++i) {
		printf(" ");
	}
}

void JsonPrintArray(Array_View<struct Json> arr, int depth) {
	printf("[ ");
	for (auto &json : arr) {
		JsonPrint(&json, depth);
		if (&json != &arr.Last())
			printf(", ");
	}
	printf(" ]");
}

void JsonPrintObject(Array_View<Json_Object::Pair> json, int depth) {
	depth += 1;
	printf("{\n");
	for (const auto &it : json) {
		JsonPrintIndent(depth);
		printf("%.*s: ", (int)it.key.length, it.key.data);
		JsonPrint(&it.value, depth);
		if (&it != &json.Last())
			printf(",\n");
	}
	printf("\n");
	JsonPrintIndent(depth - 1);
	printf("}");
}

void JsonPrint(const Json *json, int depth = 0) {
	auto type = json->type;

	if (type == JSON_TYPE_NULL) {
		printf("null");
	} else if (type == JSON_TYPE_BOOL) {
		printf("%s", json->value.boolean ? "true" : "false");
	} else if (type == JSON_TYPE_NUMBER) {
		printf("%f", json->value.number);
	} else if (type == JSON_TYPE_STRING) {
		printf("\"%.*s\"", (int)json->value.string.length, json->value.string.data);
	} else if (type == JSON_TYPE_ARRAY) {
		JsonPrintArray(json->value.array, depth);
	} else if (type == JSON_TYPE_OBJECT) {
		JsonPrintObject(json->value.object.storage, depth);
	} else {
		Unreachable();
	}
}

int main(int argc, char **argv) {
	InitThreadContext(MegaBytes(64));

	STable<int> table;

	table.Put("one", 1);
	table.Put("two", 2);
	table.Put("three", 3);

	for (auto &p : table) { printf("\t%s: %d\n", p.key.data, p.value); }

	table.Remove("one");

	for (auto &p : table) { printf("\t%s: %d\n", p.key.data, p.value); }

	table.Put("zero", 0);

	for (auto &p : table) { printf("\t%s: %d\n", p.key.data, p.value); }

	auto two = table.FindOrPut("two");

	auto five = table.Find("five");

	auto zero = table.Find("zero");

	Free(&table);

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Json parsed_json;
	bool parsed = JsonParse(NoCheckinJSON, &parsed_json);

	printf("JSON parsed: %s...\n", parsed ? "done" : "failed");

	JsonPrint(&parsed_json);
	JsonFree(&parsed_json);

	NetInit();

	Net_Socket net;
	if (NetOpenClientConnection("discord.com", "443", &net) != Net_Ok) {
		return 1;
	}

	NetPerformTSLHandshake(&net);

	const char *token = argv[1];

	String_Builder content_builder;
	Json_Builder json = JsonBuilderCreate(&content_builder);

	JsonWriteBeginObject(&json);
	JsonWriteKeyBool(&json, "tts", false);
	JsonWriteKey(&json, "embeds");

	JsonWriteBeginArray(&json);

	JsonWriteBeginObject(&json);
	JsonWriteKeyString(&json, "title", "Yahallo!");
	JsonWriteKeyString(&json, "description", "This message is generated from Katachi bot (programming in C)");
	JsonWriteEndObject(&json);

	JsonWriteEndArray(&json);
	JsonWriteEndObject(&json);


	String_Builder builder;

	String method = "POST";
	String endpoint = "/api/v9/channels/850062383266136065/messages";

	WriteFormatted(&builder, "% % HTTP/1.1\r\n", method, endpoint);
	WriteFormatted(&builder, "Authorization: Bot %\r\n", token);
	Write(&builder, "User-Agent: KatachiBot\r\n");
	Write(&builder, "Connection: keep-alive\r\n");
	WriteFormatted(&builder, "Host: %\r\n", net.info.hostname);
	WriteFormatted(&builder, "Content-Type: %\r\n", "application/json");
	WriteFormatted(&builder, "Content-Length: %\r\n", content_builder.written);
	Write(&builder, "\r\n");

	// Send header
	for (auto buk = &builder.head; buk; buk = buk->next) {
		int bytes_sent = NetWriteSecured(&net, buk->data, (int)buk->written);
	}

	// Send Content
	for (auto buk = &content_builder.head; buk; buk = buk->next) {
		int bytes_sent = NetWriteSecured(&net, buk->data, (int)buk->written);
	}

	static char buffer[4096 * 2];

	int bytes_received = NetReadSecured(&net, buffer, sizeof(buffer) - 1);
	bytes_received += NetReadSecured(&net, buffer + bytes_received, sizeof(buffer) - 1 - bytes_received);
	if (bytes_received < 1) {
		Unimplemented();
	}

	buffer[bytes_received] = 0;

	printf("\n%s\n", buffer);

	NetCloseConnection(&net);

	NetShutdown();

	return 0;
}
