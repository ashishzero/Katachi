#include "Basic.h"
#include "StringBuilder.h"
#include "Network.h"
#include "Discord.h"

#include <stdio.h>

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

constexpr uint32_t JSON_INDEX_BUCKET_COUNT = 64;
constexpr uint32_t JSON_INDEX_PER_BUCKET = 16;
constexpr uint32_t JSON_INDEX_MASK = JSON_INDEX_PER_BUCKET - 1;
constexpr uint32_t JSON_INDEX_SHIFT = 4;

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

int main(int argc, char **argv) {
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
