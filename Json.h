#pragma once
#include "Kr/KrBasic.h"

struct Json_Builder {
	struct String_Builder *builder;
	uint64_t depth;
};

Json_Builder JsonBuilderCreate(String_Builder *builder);
int JsonWriteNextElement(Json_Builder *json);
int JsonWriteBeginObject(Json_Builder *json);
int JsonWriteEndObject(Json_Builder *json);
int JsonWriteBeginArray(Json_Builder *json);
int JsonWriteEndArray(Json_Builder *json);
int JsonWriteKey(Json_Builder *json, String key);
int JsonWriteNull(Json_Builder *json);
int JsonWriteString(Json_Builder *json, String value);
int JsonWriteBool(Json_Builder *json, bool value);
int JsonWriteNumber(Json_Builder *json, int64_t value);
int JsonWriteNumber(Json_Builder *json, uint64_t value);
int JsonWriteNumber(Json_Builder *json, double value);
int JsonWriteKeyString(Json_Builder *json, String key, String value);
int JsonWriteKeyBool(Json_Builder *json, String key, bool value);
int JsonWriteKeyNumber(Json_Builder *json, String key, int64_t value);
int JsonWriteKeyNumber(Json_Builder *json, String key, uint64_t value);
int JsonWriteKeyNumber(Json_Builder *json, String key, double value);

//
//
//

enum Json_Type {
	JSON_TYPE_NULL,
	JSON_TYPE_BOOL,
	JSON_TYPE_NUMBER,
	JSON_TYPE_STRING,
	JSON_TYPE_ARRAY,
	JSON_TYPE_OBJECT
};

using Json_Bool = bool;
using Json_Number = double;
using Json_String = String;
using Json_Array = Array<struct Json>;
using Json_Object = Table<String, struct Json>;

union Json_Value {
	Json_Bool boolean;
	Json_Number number;
	Json_String string;
	Json_Array array;
	Json_Object object;
	Json_Value() {}
};

struct Json {
	Json_Type type = JSON_TYPE_NULL;
	Json_Value value;
};

void JsonFree(Json *json);
Json JsonFromArray(Json_Array arr);
Json JsonFromObject(Json_Object object);
void JsonObjectPut(Json_Object *json, String key, Json value);
void JsonObjectPutBool(Json_Object *json, String key, bool boolean);
void JsonObjectPutNumber(Json_Object *json, String key, double number);
void JsonObjectPutNumber(Json_Object *json, String key, int64_t number);
void JsonObjectPutString(Json_Object *json, String key, String string);
void JsonObjectPutArray(Json_Object *json, String key, Json_Array array);
void JsonObjectPutObject(Json_Object *json, String key, Json_Object object);
Json *JsonObjectFind(Json_Object *json, String key);

//
//
//

bool JsonParse(String json_string, Json *out_json, Memory_Allocator allocator = ThreadContext.allocator);

//
//
//

void JsonBuild(Json_Builder *builder, const Json &json);
