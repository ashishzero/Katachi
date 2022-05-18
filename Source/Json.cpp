#include "Json.h"

#include <stdlib.h>
#include <stdio.h>

void Jsonify::PushByte(uint8_t byte) {
	if (pos >= allocated) {
		void *mem = PushSize(arena, Jsonify::PUSH_SIZE);
		if (!mem) return;
		if (!start)
			start = (uint8_t *)mem;
		Assert(mem == start + allocated);
		allocated += Jsonify::PUSH_SIZE;
	}
	start[pos++] = byte;
}

void Jsonify::PushBuffer(Buffer buff) {
	if (pos + buff.length > allocated) {
		ptrdiff_t allocation_size = AlignPower2Up(Maximum(Jsonify::PUSH_SIZE, buff.length), Jsonify::PUSH_SIZE);
		void *mem = PushSize(arena, allocation_size);
		if (!mem) return;
		if (!start)
			start = (uint8_t *)mem;
		Assert(mem == start + allocated);
		allocated += allocation_size;
	}
	memcpy(start + pos, buff.data, buff.length);
	pos += buff.length;
}

void Jsonify::NextElement(bool iskey) {
	if (iskey && flags[index] & FLAG_OBJECT) {
		if (flags[index] & FLAG_REPEAT) {
			Jsonify::PushByte(',');
			return;
		}
		flags[index] |= FLAG_REPEAT;
	} else if (flags[index] & FLAG_ARRAY) {
		if (flags[index] & FLAG_REPEAT) {
			Jsonify::PushByte(',');
			return;
		}
		flags[index] |= FLAG_REPEAT;
	}
}

void Jsonify::PushScope(uint8_t flag) {
	Assert(index + 1 < ArrayCount(flags));
	index += 1;
	flags[index] = flag;
}

void Jsonify::PopScope() {
	Assert(index);
	index -= 1;
}

void Jsonify::BeginObject() {
	PushByte('{');
	PushScope(FLAG_OBJECT);
}

void Jsonify::EndObject() {
	PopScope();
	PushByte('}');
}

void Jsonify::BeginArray() {
	PushByte('[');
	PushScope(FLAG_ARRAY);
}

void Jsonify::EndArray() {
	PopScope();
	PushByte(']');
}

void Jsonify::PushKey(String key) {
	NextElement(true);
	PushByte('"');
	PushBuffer(key);
	PushByte('"');
	PushByte(':');
}

void Jsonify::PushString(String str) {
	NextElement(false);
	PushByte('"');
	PushBuffer(str);
	PushByte('"');
}

void Jsonify::PushId(uint64_t id) {
	char buff[100];
	int len = snprintf(buff, sizeof(buff), "%zu", id);
	PushByte('"');
	PushBuffer(Buffer(buff, len));
	PushByte('"');
}

void Jsonify::PushFloat(float number) {
	NextElement(false);
	char buff[100];
	int len = snprintf(buff, sizeof(buff), "%f", number);
	PushBuffer(Buffer(buff, len));
}

void Jsonify::PushInt(int number) {
	NextElement(false);
	char buff[100];
	int len = snprintf(buff, sizeof(buff), "%d", number);
	PushBuffer(Buffer(buff, len));
}

void Jsonify::PushBool(bool boolean) {
	NextElement(false);
	String str = boolean ? String("true") : String("false");
	PushBuffer(str);
}

void Jsonify::PushNull() {
	NextElement(false);
	PushBuffer("null");
}

void Jsonify::KeyValue(String key, String value) {
	PushKey(key);
	PushString(value);
}

void Jsonify::KeyValue(String key, uint64_t value) {
	PushKey(key);
	PushId(value);
}

void Jsonify::KeyValue(String key, int value) {
	PushKey(key);
	PushInt(value);
}

void Jsonify::KeyValue(String key, float value) {
	PushKey(key);
	PushFloat(value);
}

void Jsonify::KeyValue(String key, bool value) {
	PushKey(key);
	PushBool(value);
}

void Jsonify::KeyNull(String key) {
	PushKey(key);
	PushNull();
}

String Jsonify_BuildString(Jsonify *jsonify) {
	String str(jsonify->start, jsonify->pos);
	PopSize(jsonify->arena, jsonify->allocated - jsonify->pos);
	*jsonify = Jsonify(jsonify->arena);
	return str;
}

//
//
//

void JsonFree(Json *json) {
	auto type = json->type;
	if (type == JSON_TYPE_ARRAY) {
		Free(&json->value.array);
	} else if (type == JSON_TYPE_OBJECT) {
		for (auto &item : json->value.object) {
			JsonFree(&item.value);
		}
		Free(&json->value.object);
	} else if (type == JSON_TYPE_STRING) {
		MemoryFree(json->value.string.value.data, json->value.string.value.length, json->value.string.allocator);
	}
}

//
//
//


bool JsonGetBool(const Json &json, bool def) {
	if (json.type == JSON_TYPE_BOOL)
		return json.value.boolean;
	return def;
}

float JsonGetFloat(const Json &json, float def) {
	if (json.type == JSON_TYPE_NUMBER)
		return json.value.number;
	return def;
}

int JsonGetInt(const Json &json, int def) {
	if (json.type == JSON_TYPE_NUMBER)
		return (int)json.value.number;
	return def;
}

String JsonGetString(const Json &json, String def) {
	if (json.type == JSON_TYPE_STRING)
		return json.value.string.value;
	return def;
}

Json_Array JsonGetArray(const Json &json, Json_Array def) {
	if (json.type == JSON_TYPE_ARRAY)
		return json.value.array;
	return def;
}

Json_Object JsonGetObject(const Json &json, Json_Object def) {
	if (json.type == JSON_TYPE_OBJECT)
		return json.value.object;
	return def;
}

Json JsonGet(const Json_Object &obj, const String key, Json def) {
	const Json *elem = obj.Find(key);
	if (elem)
		return *elem;
	return def;
}

bool JsonGetBool(const Json_Object &obj, const String key, bool def) {
	const Json *elem = obj.Find(key);
	if (elem)
		return JsonGetBool(*elem);
	return def;
}

float JsonGetFloat(const Json_Object &obj, const String key, float def) {
	const Json *elem = obj.Find(key);
	if (elem)
		return JsonGetFloat(*elem);
	return def;
}

int JsonGetInt(const Json_Object &obj, const String key, int def) {
	const Json *elem = obj.Find(key);
	if (elem)
		return JsonGetInt(*elem);
	return def;
}

String JsonGetString(const Json_Object &obj, const String key, String def) {
	const Json *elem = obj.Find(key);
	if (elem)
		return JsonGetString(*elem);
	return def;
}

Json_Array JsonGetArray(const Json_Object &obj, const String key, Json_Array def) {
	const Json *elem = obj.Find(key);
	if (elem)
		return JsonGetArray(*elem);
	return def;
}

Json_Object JsonGetObject(const Json_Object &obj, const String key, Json_Object def) {
	const Json *elem = obj.Find(key);
	if (elem)
		return JsonGetObject(*elem);
	return def;
}

//
//
//

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
	String          content;
	String          identifier;
	float           number;
};

struct Json_Tokenizer {
	String buffer;
	uint8_t *current;
	Json_Token token;
};

static Json_Tokenizer JsonTokenizerBegin(String content) {
	Json_Tokenizer tokenizer;
	tokenizer.buffer = content;
	tokenizer.current = content.data;
	return tokenizer;
}

static bool JsonTokenizerEnd(Json_Tokenizer *tokenizer) {
	return tokenizer->current == (tokenizer->buffer.data + tokenizer->buffer.length);
}

static inline bool JsonIsNumber(uint32_t value) {
	return (value >= '0' && value <= '9') || value == '-' || value == '+';
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

static bool JsonTokenize(Json_Tokenizer *tokenizer) {
	uint8_t *last = tokenizer->buffer.data + tokenizer->buffer.length;
	while (tokenizer->current < last) {
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
			char buffer[128 + 1];

			buffer[0] = value;
			int pos = 1;
			for (tokenizer->current += 1; tokenizer->current < last; ++tokenizer->current) {
				if (JsonIsNumber(*tokenizer->current) || *tokenizer->current == '.') {
					if (pos >= sizeof(buffer) - 1) return false;
					buffer[pos++] = *tokenizer->current;
				} else {
					break;
				}
			}
			buffer[pos] = 0;

			char *str_end           = nullptr;
			tokenizer->token.number = (float)strtod(buffer, &str_end);

			if (str_end != buffer + pos)
				return false;

			tokenizer->token.kind    = JSON_TOKEN_NUMBER;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);
			return true;
		}

		if (value == '"') {
			tokenizer->current += 1;

			bool proper_string = false;
			for (; tokenizer->current < last; ++tokenizer->current) {
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
			for (int i = 1; tokenizer->current < last && i < True.length; ++i) {
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
			for (int i = 1; tokenizer->current < last && i < False.length; ++i) {
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
			for (int i = 1; tokenizer->current < last && i < Null.length; ++i) {
				if (*tokenizer->current != Null[i])
					return false;
				tokenizer->current += 1;
			}

			tokenizer->token.kind = JSON_TOKEN_NULL;
			tokenizer->token.content = JsonTokenizerMakeTokenContent(tokenizer, start);

			return true;
		}

		return false;
	}

	return false;
}

struct Json_Parser {
	Json_Tokenizer tokenizer;
	Memory_Allocator allocator;
	Json *parsed_json;
	bool parsing;
};

static Json_Parser JsonParserBegin(String content, Json *json, Memory_Allocator allocator) {
	Json_Parser parser;
	parser.tokenizer = JsonTokenizerBegin(content);
	parser.allocator = allocator;
	parser.parsed_json = json;
	parser.parsing = true;
	return parser;
}

static bool JsonParserEnd(Json_Parser *parser) {
	return JsonTokenizerEnd(&parser->tokenizer);
}

static bool JsonParsing(Json_Parser *parser) {
	return parser->parsing;
}

static bool JsonParsePeekToken(Json_Parser *parser, Json_Token_Kind token) {
	return parser->tokenizer.token.kind == token;
}

static Json_Token JsonParseGetToken(Json_Parser *parser) {
	return parser->tokenizer.token;
}

static bool JsonParseAcceptToken(Json_Parser *parser, Json_Token_Kind token, Json_Token *out = nullptr) {
	if (JsonParsePeekToken(parser, token)) {
		if (out) {
			*out = parser->tokenizer.token;
		}
		parser->parsing = JsonTokenize(&parser->tokenizer);
		return true;
	}
	return false;
}

static bool JsonParseExpectToken(Json_Parser *parser, Json_Token_Kind token, Json_Token *out = nullptr) {
	if (JsonParseAcceptToken(parser, token, out)) {
		return true;
	}
	parser->parsing = false;
	return false;
}

static bool JsonParseObject(Json_Parser *parser, Json *json);
static bool JsonParseArray(Json_Parser *parser, Json *json);

static Json_String JsonNormalizeString(Json_Parser *parser, String input) {
	Array<uint8_t> string(parser->allocator);
	string.Resize(input.length);

	ptrdiff_t length = 0;
	for (ptrdiff_t index = 0; index < input.length; ++index) {
		if (input[index] != '\\') {
			string[length++] = input[index];
		} else if (index + 1 < input.length && input[index + 1] == '"') {
			string[length++] = '"';
			index += 1;
		} else {
			string[length++] = input[index];
		}

	}

	string.count = length;
	string.Pack();

	Json_String result;
	result.allocator = string.allocator;
	result.value     = String(string.data, string.count);
	return result;
}

static bool JsonParseValue(Json_Parser *parser, Json *json) {
	if (JsonParsing(parser)) {
		if (JsonParsePeekToken(parser, JSON_TOKEN_OPEN_CURLY_BRACKET)) {
			if (JsonParseObject(parser, json)) {
				return true;
			}
			return false;
		}

		if (JsonParsePeekToken(parser, JSON_TOKEN_OPEN_SQUARE_BRACKET)) {
			if (JsonParseArray(parser, json)) {
				return true;
			}
			return false;
		}

		Json_Token token;

		if (JsonParseAcceptToken(parser, JSON_TOKEN_NUMBER, &token)) {
			json->type = JSON_TYPE_NUMBER;
			json->value.number = token.number;
			return true;
		}

		if (JsonParseAcceptToken(parser, JSON_TOKEN_IDENTIFIER, &token)) {
			json->type = JSON_TYPE_STRING;
			json->value.string = JsonNormalizeString(parser, token.identifier);
			return true;
		}

		if (JsonParseAcceptToken(parser, JSON_TOKEN_TRUE, &token)) {
			json->type = JSON_TYPE_BOOL;
			json->value.boolean = true;
			return true;
		}

		if (JsonParseAcceptToken(parser, JSON_TOKEN_FALSE, &token)) {
			json->type = JSON_TYPE_BOOL;
			json->value.boolean = false;
			return true;
		}

		if (JsonParseAcceptToken(parser, JSON_TOKEN_NULL)) {
			json->type = JSON_TYPE_NULL;
			return true;
		}
	}
	return false;
}

static bool JsonParseKeyValuePair(Json_Parser *parser, String *key, Json *value) {
	if (JsonParsing(parser)) {
		if (JsonParseExpectToken(parser, JSON_TOKEN_IDENTIFIER)) {
			auto token = JsonParseGetToken(parser);
			*key = token.identifier;
			if (JsonParseExpectToken(parser, JSON_TOKEN_COLON)) {
				if (JsonParseValue(parser, value)) {
					return true;
				}
			}
		}
	}
	return false;
}

static bool JsonParseBody(Json_Parser *parser, Json_Object *json) {
	bool parsed = false;
	if (JsonParsing(parser)) {
		while (JsonParsing(parser)) {
			String key;
			Json value;
			if (JsonParseKeyValuePair(parser, &key, &value)) {
				json->Put(key, value);
				if (!JsonParseAcceptToken(parser, JSON_TOKEN_COMMA)) {
					json->storage.Pack();
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

static bool JsonParseObject(Json_Parser *parser, Json *json) {
	if (JsonParsing(parser)) {
		if (JsonParseExpectToken(parser, JSON_TOKEN_OPEN_CURLY_BRACKET)) {
			if (JsonParseAcceptToken(parser, JSON_TOKEN_CLOSE_CURLY_BRACKET))
				return true;

			json->type = JSON_TYPE_OBJECT;
			json->value.object = Json_Object(parser->allocator);

			if (JsonParseBody(parser, &json->value.object)) {
				return JsonParseExpectToken(parser, JSON_TOKEN_CLOSE_CURLY_BRACKET);
			}
		}
	}
	return false;
}

static bool JsonParseArray(Json_Parser *parser, Json *json) {
	if (JsonParsing(parser)) {
		if (JsonParseExpectToken(parser, JSON_TOKEN_OPEN_SQUARE_BRACKET)) {

			json->type = JSON_TYPE_ARRAY;
			Array<Json> elements(parser->allocator);

			bool parsed = false;
			while (JsonParsing(parser)) {
				if (JsonParseAcceptToken(parser, JSON_TOKEN_CLOSE_SQUARE_BRACKET)) {
					parsed = true;
					break;
				}

				Json value;
				if (JsonParseValue(parser, &value)) {
					elements.Add(value);
					if (!JsonParseAcceptToken(parser, JSON_TOKEN_COMMA)) {
						parsed = JsonParseExpectToken(parser, JSON_TOKEN_CLOSE_SQUARE_BRACKET);
						break;
					}
				}
				else {
					break;
				}
			}

			elements.Pack();
			json->value.array = elements;

			return parsed;
		}
	}
	return false;
}

static bool JsonParseRoot(Json_Parser *parser) {
	parser->parsing = JsonTokenize(&parser->tokenizer);

	if (JsonParsing(parser)) {
		if (JsonParsePeekToken(parser, JSON_TOKEN_OPEN_CURLY_BRACKET)) {
			return JsonParseObject(parser, parser->parsed_json);
		}

		if (JsonParsePeekToken(parser, JSON_TOKEN_OPEN_SQUARE_BRACKET)) {
			return JsonParseArray(parser, parser->parsed_json);
		}
	}

	return false;
}

bool JsonParse(String json_string, Json *out_json, Memory_Allocator allocator) {
	Json_Parser parser = JsonParserBegin(json_string, out_json, allocator);
	bool parsed = JsonParseRoot(&parser);
	parsed = JsonParserEnd(&parser);

	if (!parsed) {
		JsonFree(out_json);
		*out_json = {};
	}
	return parsed;
}
