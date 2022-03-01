#include "Json.h"
#include "StringBuilder.h"

#include <stdlib.h>

Json_Builder JsonBuilderCreate(String_Builder *builder) {
	Json_Builder json;
	json.builder = builder;
	json.depth = 0;
	return json;
}

int JsonWriteNextElement(Json_Builder *json) {
	int written = 0;
	if (json->depth & 0x1) {
		written = Write(json->builder, ",");
	}
	else {
		json->depth |= 0x1;
	}
	return written;
}

int JsonWriteBeginObject(Json_Builder *json) {
	Assert((json->depth & 0x8000000000000000) == 0);
	int written = JsonWriteNextElement(json);
	json->depth = json->depth << 1;
	return written + Write(json->builder, "{");
}

int JsonWriteEndObject(Json_Builder *json) {
	json->depth = json->depth >> 1;
	return Write(json->builder, "}");
}

int JsonWriteBeginArray(Json_Builder *json) {
	Assert((json->depth & 0x8000000000000000) == 0);
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

int JsonWriteNull(Json_Builder *json) {
	int written = Write(json->builder, "null");
	json->depth |= 0x1;
	return written;
}

int JsonWriteString(Json_Builder *json, String value) {
	int written = WriteFormatted(json->builder, "\"%\"", value);
	json->depth |= 0x1;
	return written;
}

int JsonWriteBool(Json_Builder *json, bool value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

int JsonWriteNumber(Json_Builder *json, int64_t value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

int JsonWriteNumber(Json_Builder *json, uint64_t value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

int JsonWriteNumber(Json_Builder *json, double value) {
	int written = Write(json->builder, value);
	json->depth |= 0x1;
	return written;
}

int JsonWriteKeyString(Json_Builder *json, String key, String value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += JsonWriteString(json, value);
	return written;
}

int JsonWriteKeyBool(Json_Builder *json, String key, bool value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += JsonWriteBool(json, value);
	return written;
}

int JsonWriteKeyNumber(Json_Builder *json, String key, int64_t value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += JsonWriteNumber(json, value);
	return written;
}

int JsonWriteKeyNumber(Json_Builder *json, String key, uint64_t value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += JsonWriteNumber(json, value);
	return written;
}

int JsonWriteKeyNumber(Json_Builder *json, String key, double value) {
	int written = 0;
	written += JsonWriteKey(json, key);
	written += JsonWriteNumber(json, value);
	return written;
}

//
//
//

void JsonFree(Json *json) {
	auto type = json->type;
	if (type == JSON_TYPE_ARRAY) {
		Free(&json->value.array);
	}
	else if (type == JSON_TYPE_OBJECT) {
		for (auto &item : json->value.object) {
			JsonFree(&item.value);
		}
		Free(&json->value.object);
	}
}

Json JsonFromArray(Json_Array arr) {
	Json json;
	json.type = JSON_TYPE_ARRAY;
	json.value.array = arr;
	return json;
}

Json JsonFromObject(Json_Object object) {
	Json json;
	json.type = JSON_TYPE_OBJECT;
	json.value.object = object;
	return json;
}

void JsonObjectPut(Json_Object *json, String key, Json value) {
	auto prev = json->FindOrPut(key);
	if (prev->type != JSON_TYPE_NULL)
		JsonFree(prev);
	*prev = value;
	json->Put(key, value);
}

void JsonObjectPutBool(Json_Object *json, String key, bool boolean) {
	Json value;
	value.type = JSON_TYPE_BOOL;
	value.value.boolean = boolean;
	JsonObjectPut(json, key, value);
}

void JsonObjectPutNumber(Json_Object *json, String key, double number) {
	Json value;
	value.type = JSON_TYPE_NUMBER;
	value.value.number = number;
	JsonObjectPut(json, key, value);
}

void JsonObjectPutNumber(Json_Object *json, String key, int64_t number) {
	Json value;
	value.type = JSON_TYPE_NUMBER;
	value.value.number = (double)number;
	JsonObjectPut(json, key, value);
}

void JsonObjectPutString(Json_Object *json, String key, String string) {
	Json value;
	value.type = JSON_TYPE_STRING;
	value.value.string = string;
	JsonObjectPut(json, key, value);
}

void JsonObjectPutArray(Json_Object *json, String key, Json_Array array) {
	Json value;
	value.type = JSON_TYPE_ARRAY;
	value.value.array = array;
	JsonObjectPut(json, key, value);
}

void JsonObjectPutObject(Json_Object *json, String key, Json_Object object) {
	Json value;
	value.type = JSON_TYPE_OBJECT;
	value.value.object = object;
	JsonObjectPut(json, key, value);
}

Json *JsonObjectFind(Json_Object *json, String key) {
	return json->Find(key);
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
	String content;
	String identifier;
	double number;
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
			json->value.string = token.identifier;
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
				JsonObjectPut(json, key, value);
				if (!JsonParseAcceptToken(parser, JSON_TOKEN_COMMA)) {
					parsed = true;
					break;
				}
			}
			else {
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

//
//
//

void JsonBuild(Json_Builder *builder, const Json &json) {
	auto type = json.type;

	if (type == JSON_TYPE_NULL)
		JsonWriteNull(builder);
	else if (type == JSON_TYPE_BOOL)
		JsonWriteBool(builder, json.value.boolean);
	else if (type == JSON_TYPE_NUMBER)
		JsonWriteNumber(builder, json.value.number);
	else if (type == JSON_TYPE_STRING)
		JsonWriteString(builder, json.value.string);
	else if (type == JSON_TYPE_ARRAY) {
		JsonWriteBeginArray(builder);
		for (auto &e : json.value.array)
			JsonBuild(builder, e);
		JsonWriteEndArray(builder);
	}
	else if (type == JSON_TYPE_OBJECT) {
		JsonWriteBeginObject(builder);
		for (auto &p : json.value.object) {
			JsonWriteKey(builder, p.key);
			JsonBuild(builder, p.value);
		}
		JsonWriteEndObject(builder);
	}
	else {
		Unreachable();
	}
}
