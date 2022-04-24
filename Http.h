#pragma once
#include "Network.h"

static constexpr int HTTP_MAX_HEADER_SIZE = KiloBytes(8);
static constexpr int HTTP_READ_CHUNK_SIZE = HTTP_MAX_HEADER_SIZE;

static_assert(HTTP_MAX_HEADER_SIZE >= HTTP_READ_CHUNK_SIZE, "");

enum Http_Connection {
	HTTP_DEFAULT,
	HTTP_CONNECTION,
	HTTPS_CONNECTION
};

enum Http_Header_Id {
	HTTP_HEADER_CACHE_CONTROL,
	HTTP_HEADER_CONNECTION,
	HTTP_HEADER_DATE,
	HTTP_HEADER_KEEP_ALIVE,
	HTTP_HEADER_PRAGMA,
	HTTP_HEADER_TRAILER,
	HTTP_HEADER_TRANSFER_ENCODING,
	HTTP_HEADER_UPGRADE,
	HTTP_HEADER_VIA,
	HTTP_HEADER_WARNING,
	HTTP_HEADER_ALLOW,
	HTTP_HEADER_CONTENT_LENGTH,
	HTTP_HEADER_CONTENT_TYPE,
	HTTP_HEADER_CONTENT_ENCODING,
	HTTP_HEADER_CONTENT_LANGUAGE,
	HTTP_HEADER_CONTENT_LOCATION,
	HTTP_HEADER_CONTENT_MD5,
	HTTP_HEADER_CONTENT_RANGE,
	HTTP_HEADER_EXPIRES,
	HTTP_HEADER_LAST_MODIFIED,
	HTTP_HEADER_ACCEPT,
	HTTP_HEADER_ACCEPT_CHARSET,
	HTTP_HEADER_ACCEPT_ENCODING,
	HTTP_HEADER_ACCEPT_LANGUAGE,
	HTTP_HEADER_AUTHORIZATION,
	HTTP_HEADER_COOKIE,
	HTTP_HEADER_EXPECT,
	HTTP_HEADER_FROM,
	HTTP_HEADER_HOST,
	HTTP_HEADER_IF_MATCH,
	HTTP_HEADER_IF_MODIFIED_SINCE,
	HTTP_HEADER_IF_NONE_MATCH,
	HTTP_HEADER_IF_RANGE,
	HTTP_HEADER_IF_UNMODIFIED_SINCE,
	HTTP_HEADER_MAX_FORWARDS,
	HTTP_HEADER_PROXY_AUTHORIZATION,
	HTTP_HEADER_REFERER,
	HTTP_HEADER_RANGE,
	HTTP_HEADER_TE,
	HTTP_HEADER_TRANSLATE,
	HTTP_HEADER_USER_AGENT,
	HTTP_HEADER_REQUEST_MAXIMUM,
	HTTP_HEADER_ACCEPT_RANGES,
	HTTP_HEADER_AGE,
	HTTP_HEADER_ETAG,
	HTTP_HEADER_LOCATION,
	HTTP_HEADER_PROXY_AUTHENTICATE,
	HTTP_HEADER_RETRY_AFTER,
	HTTP_HEADER_SERVER,
	HTTP_HEADER_SETCOOKIE,
	HTTP_HEADER_VARY,
	HTTP_HEADER_WWW_AUTHENTICATE,
	HTTP_HEADER_RESPONSE_MAXIMUM,
	HTTP_HEADER_MAXIMUM,

	_HTTP_HEADER_COUNT
};

struct Http_Header_Raw {
	String           name;
	String           value;
	Http_Header_Raw *next;
};

struct Http_Header {
	String known[_HTTP_HEADER_COUNT];
	Http_Header_Raw *raw;
};

enum Http_Version : uint32_t {
	HTTP_VERSION_1_0,
	HTTP_VERSION_1_1,
};

struct Http_Status {
	Http_Version version;
	uint32_t     code;
	String       name;
};

typedef Buffer(*Http_Body_Reader_Proc)(void *content);
typedef void(*Http_Body_Writer_Proc)(const Http_Header &header, uint8_t *buffer, int length, void *context);
struct Http_Body_Reader { Http_Body_Reader_Proc proc; void *context; };
struct Http_Body_Writer { Http_Body_Writer_Proc proc; void *context; };

struct Http_Request {
	struct Body {
		enum Type { STATIC, STREAM };
		struct Data {
			Http_Body_Reader reader;
			Buffer           buffer;
			Data() {}
		};
		Type type;
		Data data;
	};

	Http_Header      headers;
	Body             body;
	Temporary_Memory memory;
};

struct Http_Response {
	Http_Status      status;
	Http_Header      headers;
	Buffer           body;
	Temporary_Memory memory;
};

Net_Socket *Http_Connect(const String hostname, Http_Connection connection = HTTP_DEFAULT, Memory_Allocator allocator = ThreadContext.allocator);
void        Http_Disconnect(Net_Socket *http);

Http_Request * Http_CreateRequest(Net_Socket *http, Memory_Arena *arena);
void           Http_DestroyRequest(Http_Request *req);
Http_Response *Http_CreateResponse(Memory_Arena *arena);
void           Http_DestroyResponse(Http_Response *req);

Http_Response *Http_SendCustomMethod(Net_Socket *http, const String method, const String endpoint, Http_Request *req, Http_Body_Writer *writer = nullptr);
Http_Response *Http_Get(Net_Socket *http, const String endpoint, Http_Request *req, Http_Body_Writer *writer = nullptr);
Http_Response *Http_Post(Net_Socket *http, const String endpoint, Http_Request *req, Http_Body_Writer *writer = nullptr);
Http_Response *Http_Put(Net_Socket *http, const String endpoint, Http_Request *req, Http_Body_Writer *writer = nullptr);

template <typename T> Memory_Arena *Http_GetMemoryArena(T *r) { return r->memory.arena; }
template <typename T> Http_Header &Http_GetHeaderData(T *r) { return r->headers; }
template <typename T> void Http_SetHeader(T *r, Http_Header_Id id, const String value) {
	r->headers.known[id] = value;
}
template <typename T> void Http_SetContentLength(T *r, ptrdiff_t length) {
	String content_len = FmtStr(r->memory.arena, "%lld", length);
	Http_SetHeader(r, HTTP_HEADER_CONTENT_LENGTH, content_len);
}
template <typename T> void Http_SetContent(T *r, const String type, const Buffer content) {
	r->body.type = T::Body::STATIC;
	r->body.data.buffer = content;
	Http_SetHeader(r, HTTP_HEADER_CONTENT_TYPE, type);
	Http_SetContentLength(r, content.length);
}
template <typename T> void Http_SetReader(T *r, const String type, Http_Body_Reader_Proc reader, void *context) {
	r->body.type = T::Body::STREAM;
	r->body.data.reader.proc = reader;
	r->body.data.reader.context = context;
	Http_SetHeader(r, HTTP_HEADER_CONTENT_TYPE, type);
}
template <typename T> bool Http_SetCustomHeader(T *r, String name, String value) {
	Http_Header_Raw *raw = PushType(r->memory.arena, Http_Header_Raw);
	if (raw) {
		raw->name = name;
		raw->value = value;
		raw->next = r->headers.raw;
		r->headers.raw = raw;
		return true;
	}
	return false;
}
template <typename T> String Http_GetHeader(T *r, Http_Header_Id id) {
	return r->headers.known[id];
}
template <typename T> String Http_GetCustomHeader(T *r, String name) {
	for (Http_Header_Raw *raw = r->headers.raw; raw; raw = raw->next) {
		if (StrMatchCaseInsensitive(raw->name, name))
			return raw->value;
	}
	return "";
}
