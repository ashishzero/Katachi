#pragma once
#include "Kr/KrBasic.h"

struct Jsonify {
	static constexpr int PUSH_SIZE = KiloBytes(4);

	enum { FLAG_REPEAT = 0x1, FLAG_ARRAY = 0x2, FLAG_OBJECT = 0x4 };

	uint8_t * start     = nullptr;
	ptrdiff_t pos       = 0;
	ptrdiff_t allocated = 0;

	uint8_t   flags[2048];
	int       index = 0;

	Memory_Arena *arena = ThreadScratchpad();

	Jsonify() = default;
	Jsonify(Memory_Arena *_arena) : arena(_arena) {}

	void PushByte(uint8_t byte);
	void PushBuffer(Buffer buff);
	void NextElement(bool iskey);
	void PushScope(uint8_t flag);
	void PopScope();
	void BeginObject();
	void EndObject();
	void BeginArray();
	void EndArray();
	void PushKey(String key);
	void PushString(String str);
	void PushId(uint64_t id);
	void PushFloat(float number);
	void PushInt(int number);
	void PushBool(bool boolean);
	void PushNull();
	void KeyValue(String key, String value);
	void KeyValue(String key, uint64_t value);
	void KeyValue(String key, int value);
	void KeyValue(String key, float value);
	void KeyValue(String key, bool value);
	void KeyNull(String key);
};

String Jsonify_BuildString(Jsonify *jsonify);

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

struct Json;
typedef Array<Json> Json_Array;
typedef Hash_Table<String, Json> Json_Object;

union Json_Value {
	bool        boolean;
	float       number;
	String      string;
	Json_Array  array;
	Json_Object object;
	Json_Value() {}
};

struct Json {
	Json_Type  type;
	Json_Value value;

	Json() : type(JSON_TYPE_NULL){}
	explicit Json(bool val) : type(JSON_TYPE_BOOL) { value.boolean = val; }
	explicit Json(float num) : type(JSON_TYPE_NUMBER) { value.number = num; }
	explicit Json(int num) : type(JSON_TYPE_NUMBER) { value.number = (float)num; }
	explicit Json(String str) : type(JSON_TYPE_STRING) { value.string = str; }
	explicit Json(Json_Array arr) : type(JSON_TYPE_ARRAY) { value.array = arr; }
	explicit Json(Json_Object obj) : type(JSON_TYPE_OBJECT) { value.object = obj; }
};

void JsonFree(Json *json);

bool        JsonGetBool(const Json &json, bool def = false);
float       JsonGetFloat(const Json &json, float def = 0.0f);
int         JsonGetInt(const Json &json, int def = 0);
String      JsonGetString(const Json &json, String def = String());
Json_Array  JsonGetArray(const Json &json, Json_Array def = Json_Array());
Json_Object JsonGetObject(const Json &json, Json_Object def = Json_Object());

Json        JsonGet(const Json_Object &obj, const String key, Json def = Json());
bool        JsonGetBool(const Json_Object &obj, const String key, bool def = false);
float       JsonGetFloat(const Json_Object &obj, const String key, float def = 0.0f);
int         JsonGetInt(const Json_Object &obj, const String key, int def = 0);
String      JsonGetString(const Json_Object &obj, String key, String def = String());
Json_Array  JsonGetArray(const Json_Object &obj, String key, Json_Array def = Json_Array());
Json_Object JsonGetObject(const Json_Object &obj, String key, Json_Object def = Json_Object());



//
//
//

bool JsonParse(String json_string, Json *out_json, Memory_Allocator allocator = ThreadContext.allocator);
