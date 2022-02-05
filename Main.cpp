#include "Basic.h"
#include "StringBuilder.h"
#include "Network.h"
#include "Discord.h"

#include <stdio.h>
#include <stdlib.h>

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//

struct Json_Builder {
	String_Builder *builder;
	uint64_t depth;
};

int JsonWriteNextElement(Json_Builder *json) {
	int written = 0;
	if (json->depth & 0x1) {
		written = Write(json->builder, ",");
	} else {
		json->depth |= 0x1;
	}
	return written;
}

int JsonWriteBeginObject(Json_Builder *json) {
	//Assert(json->depth & 0x8000000000000000 == 0);
	int written = JsonWriteNextElement(json);
	json->depth = json->depth << 1;
	return written + Write(json->builder, "{");
}

int JsonWriteEndObject(Json_Builder *json) {
	json->depth = json->depth >> 1;
	return Write(json->builder, "}");
}

int JsonWriteBeginArray(Json_Builder *json) {
	//Assert(json->depth & 0x8000000000000000 == 0);
	int written = JsonWriteNextElement(json);
	json->depth = json->depth << 1;
	return written + Write(json->builder, "[");
}

int JsonWriteEndArray(Json_Builder *json) {
	json->depth = json->depth >> 1;
	return Write(json->builder, "]");
}

int JsonWriteKey(Json_Builder *json, String key) {
	int written = 0;
	written += JsonWriteNextElement(json);
	written += Write(json->builder, '"');
	written += Write(json->builder, key);
	written += Write(json->builder, '"');
	written += Write(json->builder, ':');
	json->depth &= ~(uint64_t)0x1;
	return written;
}

template <typename Type>
int JsonWriteKeyValue(Json_Builder *json, String key, Type value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += WriteFormatted(json->builder, "\"%\"", value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonWriteKeyValue(Json_Builder *json, String key, bool value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonWriteKeyValue(Json_Builder *json, String key, int64_t value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonWriteKeyValue(Json_Builder *json, String key, uint64_t value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <typename Type>
int JsonWriteValue(Json_Builder *json, Type value) {
	int written = WriteFormatted(json->builder, "\"%\"", value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonWriteValue(Json_Builder *json, bool value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonWriteValue(Json_Builder *json, int64_t value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

template <>
int JsonWriteValue(Json_Builder *json, uint64_t value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

//
//
//

enum class Json_Type {
	BOOL, NUMBER, STRING, ARRAY, OBJECT
};

union Json_Value {
	bool boolean;
	int64_t number;
	struct {
		int64_t length;
		uint8_t *data;
	} string;
	struct {
		Json_Type type;
		Json_Value *data;
	} array;
	struct Json_Object *object;
};

struct Json_Object {	
	Json_Type type;
	Json_Value value;
};

constexpr uint32_t JSON_INDEX_BUCKET_COUNT = 8;
constexpr uint32_t JSON_INDEX_PER_BUCKET = 8;
constexpr uint32_t JSON_INDEX_MASK = JSON_INDEX_PER_BUCKET - 1;
constexpr uint32_t JSON_INDEX_SHIFT = 3;

struct Json_Pair {
	String *key;
	Json_Object *value;
};

struct Json {	
	struct Index_Bucket {
		uint32_t hashes[JSON_INDEX_PER_BUCKET] = {};
		uint32_t indices[JSON_INDEX_PER_BUCKET] = {};
		Index_Bucket *next = nullptr;
	};

	struct Index_Table {
		Index_Bucket buckets[JSON_INDEX_BUCKET_COUNT];
	};

	Index_Table index;
	Array<String> keys;
	Array<Json_Object> values;

	Memory_Allocator allocator = ThreadContext.allocator;

	Json() = default;
	Json(Memory_Allocator alloc): keys(alloc), values(alloc), allocator(alloc){}
};

//
//
//

Json_Pair IterBegin(Json *json) {
	return Json_Pair { json->keys.data, json->values.data };
}

void IterNext(Json_Pair *iter) {
	iter->key += 1;
	iter->value += 1;
}

bool IterEnd(Json *json, Json_Pair iter) {
	return iter.key->data < (json->keys.data + json->keys.count)->data;
}

static inline uint32_t Murmur32Scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

static uint32_t Murmur3Hash32(const uint8_t* key, size_t len, uint32_t seed) {
	uint32_t h = seed;
    uint32_t k;
    
	for (size_t i = len >> 2; i; i--) {
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= Murmur32Scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    
	k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    h ^= Murmur32Scramble(k);

	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

uint32_t JsonGetHashValue(String key) {
	uint32_t hash = Murmur3Hash32(key.data, key.length, 0x42690042);
	return hash ? hash : 1;
}

void JsonPutObject(Json *json, String key, Json_Object value) {
	uint32_t hash = JsonGetHashValue(key);

	uint32_t pos = hash & (JSON_INDEX_BUCKET_COUNT - 1);
	uint32_t bucket_index = pos >> JSON_INDEX_SHIFT;

	for (auto bucket = &json->index.buckets[bucket_index]; bucket; bucket = bucket->next) {
		uint32_t count = 0;
		for (uint32_t index = pos & JSON_INDEX_MASK; count < JSON_INDEX_PER_BUCKET; index = (index + 1) & JSON_INDEX_MASK) {
			uint32_t current_hash = bucket->hashes[index];

			if (current_hash == hash) {
				uint32_t offset = bucket->indices[index];
				//MemoryFree(json->values[offset].value.data, json->allocator);
				json->values[offset] = value;	
				return;
			} else if (current_hash == 0) {
				uint32_t offset = (uint32_t)json->values.count;
				bucket->indices[index] = offset;
				bucket->hashes[index] = hash;
				json->keys.Add(key);
				json->values.Add(Json_Object{value});
				return;
			}

			count += 1;
		}

		if (bucket->next == nullptr) {
			bucket->next = new(json->allocator) Json::Index_Bucket;
		}
	}
}

template <int64_t _Length>
void JsonPut(Json *json, String key, const char(&a)[_Length]) {
	Json_Object object;
	object.type = Json_Type::STRING;
	object.value.string.length = _Length;
	object.value.string.data = (uint8_t *)a;
	JsonPutObject(json, key, object);
}

void JsonPut(Json *json, String key, const char *value) {
	Json_Object object;
	object.type = Json_Type::STRING;
	object.value.string.length = strlen(value);
	object.value.string.data = (uint8_t *)value;
	JsonPutObject(json, key, object);
}

void JsonPut(Json *json, String key, String value) {
	Json_Object object;
	object.type = Json_Type::STRING;
	object.value.string.length = value.length;
	object.value.string.data = value.data;
	JsonPutObject(json, key, object);
}

void JsonPut(Json *json, String key, int64_t value) {
	Json_Object object;
	object.type = Json_Type::NUMBER;
	object.value.number = value;
	JsonPutObject(json, key, object);
}

void JsonPut(Json *json, String key, bool value) {
	Json_Object object;
	object.type = Json_Type::BOOL;
	object.value.boolean = value;
	JsonPutObject(json, key, object);
}

Json_Object *JsonGet(Json *json, String key) {
	uint32_t hash = JsonGetHashValue(key);
	uint32_t pos = hash & (JSON_INDEX_BUCKET_COUNT - 1);
	uint32_t bucket_index = pos >> JSON_INDEX_SHIFT;

	for (auto bucket = &json->index.buckets[bucket_index]; bucket; bucket = bucket->next) {
		uint32_t count = 0;
		for (uint32_t index = pos & JSON_INDEX_MASK; count < JSON_INDEX_PER_BUCKET; index = (index + 1) & JSON_INDEX_MASK) {
			uint32_t current_hash = bucket->hashes[index];

			if (current_hash == hash) {
				uint32_t offset = bucket->indices[index];
				return &json->values[offset];
			}

			count += 1;
		}
	}

	return nullptr;
}

enum Json_Token_Kind {
	JSON_TOKEN_OPEN_CURLY_BRACKET,
	JSON_TOKEN_CLOSE_CURLY_BRACKET,
	JSON_TOKEN_OPEN_SQUARE_BRACKET,
	JSON_TOKEN_CLOSE_SQUARE_BRACKET,
	JSON_TOKEN_COLON,
	JSON_TOKEN_COMMA,
	JSON_TOKEN_NUMBER,
	JSON_TOKEN_IDENTIFIER,
	JSON_TOKEN_TRUE,
	JSON_TOKEN_FALSE,
	JSON_TOKEN_NULL,

	_JSON_TOKEN_COUNT
};

struct Json_Token {
	Json_Token_Kind kind;
	String content;
	String identifier;
	double number;
};

struct Json_Tokenizer {
	String buffer;
	uint8_t *current;
	Json_Token token;
};

Json_Tokenizer JsonTokenizerBegin(String content) {
	Json_Tokenizer tokenizer;
	tokenizer.buffer = content;
	tokenizer.current = content.data;
	return tokenizer;
}

bool JsonTokenizerEnd(Json_Tokenizer *tokenizer) {
	return tokenizer->current == (tokenizer->buffer.data + tokenizer->buffer.length);
}

static inline bool JsonIsNumber(uint32_t value) {
	return value >= '0' && value <= '9';
}

static inline bool JsonIsWhitespace(uint32_t value) {
	return (value == ' ' || value == '\t' || value == '\n' || value == '\r');
}

static inline String JsonTokenizerMakeTokenContent(Json_Tokenizer *tokenizer, uint8_t *start) {
	String content;
	content.data = start;
	content.length = tokenizer->current - start;
	return content;
}

bool JsonTokenize(Json_Tokenizer *tokenizer) {
	while (*tokenizer->current) {
		if (JsonIsWhitespace(*tokenizer->current)) {
			tokenizer->current += 1;
			while (*tokenizer->current && JsonIsWhitespace(*tokenizer->current)) {
				tokenizer->current += 1;
			}
			continue;
		}

		uint8_t *start = tokenizer->current;
		uint8_t value = *start;

		if (value == '{') {
			tokenizer->current += 1;

			tokenizer->token.kind = JSON_TOKEN_OPEN_CURLY_BRACKET;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
			return true;
		}

		if (value == '}') {
			tokenizer->current += 1;

			tokenizer->token.kind = JSON_TOKEN_CLOSE_CURLY_BRACKET;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
			return true;
		}

		if (value == '[') {
			tokenizer->current += 1;

			tokenizer->token.kind = JSON_TOKEN_OPEN_SQUARE_BRACKET;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
			return true;
		}

		if (value == ']') {
			tokenizer->current += 1;

			tokenizer->token.kind = JSON_TOKEN_CLOSE_SQUARE_BRACKET;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
			return true;
		}

		if (value == ':') {
			tokenizer->current += 1;

			tokenizer->token.kind = JSON_TOKEN_COLON;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
			return true;
		}

		if (value == ',') {
			tokenizer->current += 1;

			tokenizer->token.kind = JSON_TOKEN_COMMA;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
			return true;
		}

		if (JsonIsNumber(value)) {
			char *str_end = nullptr;
			tokenizer->token.number = strtod((char *)tokenizer->current, &str_end);
			tokenizer->token.kind = JSON_TOKEN_NUMBER;
			tokenizer->current = (uint8_t *)str_end;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
			return true;
		}

		if (value == '"') {
			tokenizer->current += 1;

			bool proper_string = false;
			for (; *tokenizer->current; ++tokenizer->current) {
				value = *tokenizer->current;
				if (value == '"' && *(tokenizer->current - 1) != '\\') {
					tokenizer->current += 1;
					proper_string = true;
					break;
				}
			}

			if (proper_string) {
				tokenizer->token.kind = JSON_TOKEN_IDENTIFIER;
				tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
				tokenizer->token.identifier = tokenizer->token.content;
				tokenizer->token.identifier.data += 1;
				tokenizer->token.identifier.length -= 2;
				return true;
			}

			return false;
		}

		if (value == 't') {
			tokenizer->current += 1;
			const String True = "true";
			for (int i = 1; i < True.length; ++i) {
				if (*tokenizer->current != True[i])
					return false;
				tokenizer->current += 1;
			}

			tokenizer->token.kind = JSON_TOKEN_TRUE;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);

			return true;
		}

		if (value == 'f') {
			tokenizer->current += 1;
			const String False = "false";
			for (int i = 1; i < False.length; ++i) {
				if (*tokenizer->current != False[i])
					return false;
				tokenizer->current += 1;
			}

			tokenizer->token.kind = JSON_TOKEN_FALSE;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);

			return true;
		}

		if (value == 'n') {
			tokenizer->current += 1;
			const String Null = "null";
			for (int i = 1; i < Null.length; ++i) {
				if (*tokenizer->current != Null[i])
					return false;
				tokenizer->current += 1;
			}

			tokenizer->token.kind = JSON_TOKEN_TRUE;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);

			return true;
		}

		return false;
	}
	
	return false;
}

static const String NoCheckinJSON = R"foo(
{
	"reactions": [
	{
		"count": 1,
			"me": false,
			"emoji": {
			"id": null,
			"name": "í ½í´¥"
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

struct Json_Parser {
	Json_Tokenizer tokenizer;
	bool parsing;
};

bool JsonParsing(Json_Parser *parser) {
	return parser->parsing;
}

bool JsonParsePeekToken(Json_Parser *parser, Json_Token_Kind token) {
	return parser->tokenizer.token.kind == token;
}

Json_Token JsonParseGetToken(Json_Parser *parser) {
	return parser->tokenizer.token;
}

bool JsonParseAcceptToken(Json_Parser *parser, Json_Token_Kind token) {
	if (JsonParsePeekToken(parser, token)) {
		parser->parsing = JsonTokenize(&parser->tokenizer);
		return true;
	}
	return false;
}

bool JsonParseExpectToken(Json_Parser *parser, Json_Token_Kind token) {
	if (JsonParseAcceptToken(parser, token)) {
		return true;
	}
	parser->parsing = false;
	return false;
}

bool JsonParseObject(Json_Parser *parser);
bool JsonParseArray(Json_Parser *parser);

bool JsonParseValue(Json_Parser *parser) {
	if (JsonParsing(parser)) {
		if (JsonParsePeekToken(parser, JSON_TOKEN_OPEN_CURLY_BRACKET)) {
			if (JsonParseObject(parser)) {
				// got object as value
				return true;
			}
			return false;
		}

		if (JsonParsePeekToken(parser, JSON_TOKEN_OPEN_SQUARE_BRACKET)) {
			if (JsonParseArray(parser)) {
				// got array as value
				return true;
			}
			return false;
		}

		if (JsonParseAcceptToken(parser, JSON_TOKEN_NUMBER)) {
			// got number as value
			return true;
		}

		if (JsonParseAcceptToken(parser, JSON_TOKEN_IDENTIFIER)) {
			// got identifer as value
			return true;
		}

		if (JsonParseAcceptToken(parser, JSON_TOKEN_TRUE)) {
			// got true as value
			return true;
		}

		if (JsonParseAcceptToken(parser, JSON_TOKEN_FALSE)) {
			// got false as value
			return true;
		}
		
		if (JsonParseAcceptToken(parser, JSON_TOKEN_NULL)) {
			// got null as value
			return true;
		}
	}
	return false;
}

bool JsonParseKeyValuePair(Json_Parser *parser) {
	if (JsonParsing(parser)) {
		if (JsonParseExpectToken(parser, JSON_TOKEN_IDENTIFIER)) {
			auto token = JsonParseGetToken(parser);
			// got key
			auto key = token.identifier;
			if (JsonParseExpectToken(parser, JSON_TOKEN_COLON)) {
				if (JsonParseValue(parser)) {
					// got value
					return true;
				}
			}
		}
	}
	return false;
}

bool JsonParseBody(Json_Parser *parser) {
	bool parsed = false;
	if (JsonParsing(parser)) {
		while (JsonParsing(parser)) {
			if (JsonParseKeyValuePair(parser)) {
				if (!JsonParseAcceptToken(parser, JSON_TOKEN_COMMA)) {
					parsed = true;
					break;
				}
			} else {
				break;
			}
		}
	}
	return parsed;
}

bool JsonParseObject(Json_Parser *parser) {
	if (JsonParsing(parser)) {
		if (JsonParseExpectToken(parser, JSON_TOKEN_OPEN_CURLY_BRACKET)) {
			if (JsonParseAcceptToken(parser, JSON_TOKEN_CLOSE_CURLY_BRACKET))
				return true;

			if (JsonParseBody(parser)) {
				return JsonParseExpectToken(parser, JSON_TOKEN_CLOSE_CURLY_BRACKET);
			}
		}
	}
	return false;
}

bool JsonParseArray(Json_Parser *parser) {
	if (JsonParsing(parser)) {
		if (JsonParseExpectToken(parser, JSON_TOKEN_OPEN_SQUARE_BRACKET)) {

			bool parsed = false;
			while (JsonParsing(parser)) {
				if (JsonParseAcceptToken(parser, JSON_TOKEN_CLOSE_SQUARE_BRACKET)) {
					parsed = true;
					break;
				}

				if (JsonParseValue(parser)) {
					if (!JsonParseAcceptToken(parser, JSON_TOKEN_COMMA)) {
						parsed = JsonParseExpectToken(parser, JSON_TOKEN_CLOSE_SQUARE_BRACKET);
						break;
					}
				} else {
					break;
				}
			}

			return parsed;
		}
	}
	return false;
}

bool JsonParseRoot(Json_Parser *parser) {
	parser->parsing = JsonTokenize(&parser->tokenizer);

	if (JsonParsing(parser)) {
		if (JsonParsePeekToken(parser, JSON_TOKEN_OPEN_CURLY_BRACKET)) {
			return JsonParseObject(parser);
		}

		if (JsonParsePeekToken(parser, JSON_TOKEN_OPEN_SQUARE_BRACKET)) {
			return JsonParseArray(parser);
		}
	}

	return false;
}

int main() {
	Json_Parser parser;
	parser.tokenizer = JsonTokenizerBegin(NoCheckinJSON);

	bool parsed = JsonParseRoot(&parser);

	printf("JSON parsed: %s...\n", parsed ? "done" : "failed");

#if 0
	while (JsonTokenize(&tokenizer)) {
		auto token = tokenizer.token;

		if (token.kind == JSON_TOKEN_OPEN_CURLY_BRACKET)
			printf("JSON_TOKEN_OPEN_CURLY_BRACKET");
		else if (token.kind == JSON_TOKEN_CLOSE_CURLY_BRACKET)
			printf("JSON_TOKEN_CLOSE_CURLY_BRACKET");
		else if (token.kind == JSON_TOKEN_OPEN_SQUARE_BRACKET)
			printf("JSON_TOKEN_OPEN_SQUARE_BRACKET");
		else if (token.kind == JSON_TOKEN_CLOSE_SQUARE_BRACKET)
			printf("JSON_TOKEN_CLOSE_SQUARE_BRACKET");
		else if (token.kind == JSON_TOKEN_COLON)
			printf("JSON_TOKEN_COLON");
		else if (token.kind == JSON_TOKEN_COMMA)
			printf("JSON_TOKEN_COMMA");
		else if (token.kind == JSON_TOKEN_NUMBER)
			printf("JSON_TOKEN_NUMBER");
		else if (token.kind == JSON_TOKEN_IDENTIFIER)
			printf("JSON_TOKEN_IDENTIFIER");
		else if (token.kind == JSON_TOKEN_TRUE)
			printf("JSON_TOKEN_TRUE");
		else if (token.kind == JSON_TOKEN_FALSE)
			printf("JSON_TOKEN_FALSE");
		else
			Unreachable();

		printf(": %.*s", (int)token.content.length, token.content.data);

		if (token.kind == JSON_TOKEN_NUMBER) {
			printf(" (%f)", token.number);
		} else if (token.kind == JSON_TOKEN_IDENTIFIER) {
			printf(": %.*s", (int)token.identifier.length, token.identifier.data);
		}

		printf("\n");
	}

	printf("Tokenizer success: %s", JsonTokenizerEnd(&tokenizer) ? "true" : "false");
#endif
}

int main2(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	InitThreadContext(MegaBytes(64));

	NetInit();

	Net_Socket net;
	if (NetOpenClientConnection("discord.com", "443", &net) != Net_Ok) {
		return 1;
	}

	NetPerformTSLHandshake(&net);

	const char *token = argv[1];

	{
		Json json;

		const char *msg = "Yahallo!";

		JsonPut(&json, "tts", false);
		JsonPut(&json, "title", msg);
		JsonPut(&json, "description", "This message is generated from Katachi bot");

		ForEach (json) {
			printf("%s : ", it.key->data);

			if (it.value->type == Json_Type::BOOL)
				printf("%s", it.value->value.boolean ? "true" : "false");
			else if (it.value->type == Json_Type::NUMBER)
				printf("%zd", it.value->value.number);
			else if (it.value->type == Json_Type::STRING)
				printf("%.*s", (int)it.value->value.string.length, it.value->value.string.data);
			else
				Unimplemented();
			
			printf("\n");
		}

		//printf("%s = %s\n", "tts", JsonGet(&json, "tts")->value.string.data);
		//printf("%s = %s\n", "title", JsonGet(&json, "title")->value.string.data);
		//printf("%s = %s\n", "description", JsonGet(&json, "description")->value.string.data);
	}

	String_Builder content_builder;
	Json_Builder json = { &content_builder, 0 };

	JsonWriteBeginObject(&json);
	JsonWriteKeyValue(&json, "tts", false);
	JsonWriteKey(&json, "embeds");

	JsonWriteBeginArray(&json);

	JsonWriteBeginObject(&json);
	JsonWriteKeyValue(&json, "title", "Yahallo!");
	JsonWriteKeyValue(&json, "description", "This message is generated from Katachi bot (programming in C)");
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
		int bytes_sent = NetWriteSecured(&net, buk->data, buk->written);
	}

	// Send Content
	for (auto buk = &content_builder.head; buk; buk = buk->next) {
		int bytes_sent = NetWriteSecured(&net, buk->data, buk->written);
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
