#include "Kr/KrBasic.h"
#include "Network.h"
#include "Discord.h"
#include "Kr/KrString.h"

#include <stdio.h>
#include <stdlib.h>

#include "Json.h"

//
// https://discord.com/developers/docs/topics/rate-limits#rate-limits
//

static const String Message = u8R"foo(
{
  "content": "Hello, Discord!🔥🔥🔥🔥🔥",
  "tts": false,
  "embeds": [{
    "title": "Hello, Embed!",
    "description": "This is an embedded message."
  }]
}
)foo";

void LogProcedure(void *context, Log_Level level, const char *source, const char *fmt, va_list args) {
	FILE *fp = level == LOG_LEVEL_INFO ? stdout : stderr;
	fprintf(fp, "[%s] ", source);
	vfprintf(fp, fmt, args);
	fprintf(fp, "\n");
}

//
//
//

struct Builder {
	uint8_t *     mem;
	ptrdiff_t     written;
	ptrdiff_t     cap;
	ptrdiff_t     thrown;
};

void BuilderBegin(Builder *builder, void *mem, ptrdiff_t cap) {
	builder->mem         = (uint8_t *)mem;
	builder->written     = 0;
	builder->thrown      = 0;
	builder->cap         = cap;
}

String BuilderEnd(Builder *builder) {
	String buffer(builder->mem, builder->written);
	return buffer;
}

void BuilderWriteBytes(Builder *builder, void *ptr, ptrdiff_t size) {
	uint8_t *data = (uint8_t *)ptr;
	while (size > 0) {
		if (builder->written < builder->cap) {
			ptrdiff_t write_size = Minimum(size, builder->cap - builder->written);
			memcpy(builder->mem + builder->written, data, write_size);
			builder->written += write_size;
			size -= write_size;
			data += write_size;
		} else {
			builder->thrown += size;
			WriteLogErrorEx("Builder", "Buffer full. Failed to write %d bytes", size);
			return;
		}
	}
}

void BuilderWrite(Builder *builder, Buffer buffer) {
	BuilderWriteBytes(builder, buffer.data, buffer.length);
}

template <typename ...Args>
void BuilderWrite(Builder *builder, Buffer buffer, Args... args) {
	BuilderWriteBytes(builder, buffer.data, buffer.length);
	BuilderWrite(builder, args...);
}

void BuilderWriteArray(Builder *builder, Array_View<Buffer> buffers) {
	for (auto b : buffers)
		BuilderWriteBytes(builder, b.data, b.length);
}

//
//
//

#define IsNumber(a) ((a) >= '0' && (a) <= '9')

bool ParseInt(String content, ptrdiff_t *value) {
	ptrdiff_t result = 0;
	ptrdiff_t index = 0;
	for (; index < content.length && IsNumber(content[index]); ++index)
		result = (result * 10 + (content[index] - '0'));
	*value = result;
	return index == content.length;
}

bool ParseHex(String content, ptrdiff_t *value) {
	ptrdiff_t result = 0;
	ptrdiff_t index = 0;
	for (; index < content.length; ++index) {
		ptrdiff_t digit;
		uint8_t ch = content[index];
		if (IsNumber(ch))
			digit = ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			digit = 10 + (ch - 'a');
		else if (ch >= 'A' && ch <= 'F')
			digit = 10 + (ch - 'A');
		else
			break;
		result = (result * 16 + digit);
	}
	*value = result;
	return index == content.length;
}

//
// https://discord.com
//


struct Url {
	String scheme;
	String host;
	String port;
};

static bool Http_UrlExtract(String hostname, Url *url) {
	ptrdiff_t host = StrFind(hostname, "://", 0);
	if (host >= 0) {
		url->scheme     = SubStr(hostname, 0, host);
		ptrdiff_t colon = StrFind(hostname, ":", host + 1);
		if (colon >= 0) {
			url->host = SubStr(hostname, host + 3, colon - host - 3);
			url->port = SubStr(hostname, colon + 1);
		} else {
			url->host = SubStr(hostname, host + 3);
			url->port = url->scheme;
		}
	} else if (hostname.length) {
		url->host   = hostname;
		url->scheme = "http";
		url->port   = url->scheme;
	} else {
		return false;
	}
	return true;
}

enum Http_Connection {
	HTTP_DEFAULT,
	HTTP_CONNECTION,
	HTTPS_CONNECTION
};

Net_Socket *Http_Connect(const String hostname, Http_Connection connection = HTTP_DEFAULT, Memory_Allocator allocator = ThreadContext.allocator) {
	Url url;
	if (Http_UrlExtract(hostname, &url)) {
		Net_Socket *http = Net_OpenConnection(url.host, url.port, NET_SOCKET_TCP, allocator);
		if (http) {
			if ((connection == HTTP_DEFAULT && url.scheme == "https") || connection == HTTPS_CONNECTION) {
				if (Net_OpenSecureChannel(http, true))
					return http;
				Net_CloseConnection(http);
				return nullptr;
			}
			return http;
		}
		return nullptr;
	}
	WriteLogErrorEx("Http", "Invalid hostname: %*.s", (int)hostname.length, hostname.data);
	return nullptr;
}

void Http_Disconnect(Net_Socket *http) {
	Net_CloseConnection(http);
}

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

static const String HttpHeaderMap[] = {
	"Cache-Control",
	"Connection",
	"Date",
	"Keep-Alive",
	"Pragma",
	"Trailer",
	"Transfer-Encoding",
	"Upgrade",
	"Via",
	"Warning",
	"Allow",
	"Content-Length",
	"Content-Type",
	"Content-Encoding",
	"Content-Language",
	"Content-Location",
	"Content-Md5",
	"Content-Range",
	"Expires",
	"Last-Modified",
	"Accept",
	"Accept-Charset",
	"Accept-Encoding",
	"Accept-Language",
	"Authorization",
	"Cookie",
	"Expect",
	"From",
	"Host",
	"If-Match",
	"If-Modified-Since",
	"If-None-Match",
	"If-Range",
	"If-Unmodified-Since",
	"Max-Forwards",
	"Proxy-Authorization",
	"Referer",
	"Range",
	"Te",
	"Translate",
	"User-Agent",
	"Request-Maximum",
	"Accept-Ranges",
	"Age",
	"Etag",
	"Location",
	"Proxy-Authenticate",
	"Retry-After",
	"Server",
	"Setcookie",
	"Vary",
	"WWW-Authenticate",
	"Response-Maximum",
	"Maximum",
};

static_assert(_HTTP_HEADER_COUNT == ArrayCount(HttpHeaderMap), "");

struct Http_Header_Raw {
	String           name;
	String           value;
	Http_Header_Raw *next;
};

struct Http_Header {
	String known[_HTTP_HEADER_COUNT];
	Http_Header_Raw *raw;
};

typedef Buffer(*Http_Body_Reader_Proc)(void *content);

struct Http_Body_Reader {
	Http_Body_Reader_Proc proc;
	void *context;
};

typedef void(*Http_Body_Writer_Proc)(uint8_t *buffer, int length, void *context);

struct Http_Body_Writer {
	Http_Body_Writer_Proc proc;
	void *context;
};

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

Http_Request *Http_CreateRequest(Net_Socket *http, Memory_Arena *arena) {
	Temporary_Memory memory              = BeginTemporaryMemory(arena);
	Http_Request *req                    = PushTypeZero(arena, Http_Request);
	req->memory                          = memory;
	req->headers.known[HTTP_HEADER_HOST] = Net_GetHostname(http);
	return req;
}

void Http_DestroyRequest(Http_Request *req) {
	EndTemporaryMemory(&req->memory);
}

Memory_Arena *Http_RequestGetArena(Http_Request *req) {
	return req->memory.arena;
}

void Http_RequestSet(Http_Request *req, Http_Header_Id id, const String value) {
	req->headers.known[id] = value;
}

void Http_RequestSetContentLength(Http_Request *req, ptrdiff_t length) {
	String content_len = FmtStr(req->memory.arena, "%lld", length);
	Http_RequestSet(req, HTTP_HEADER_CONTENT_LENGTH, content_len);
}

void Http_RequestSetContent(Http_Request *req, const String type, const Buffer content) {
	req->body.type        = Http_Request::Body::STATIC;
	req->body.data.buffer = content;
	Http_RequestSet(req, HTTP_HEADER_CONTENT_TYPE, type);
	Http_RequestSetContentLength(req, content.length);
}

void Http_RequestSetReader(Http_Request *req, const String type, Http_Body_Reader_Proc reader, void *context) {
	req->body.type                = Http_Request::Body::STREAM;
	req->body.data.reader.proc    = reader;
	req->body.data.reader.context = context;
	Http_RequestSet(req, HTTP_HEADER_CONTENT_TYPE, type);
}

bool Http_RequestAddCustomHeader(Http_Request *req, String name, String value) {
	Http_Header_Raw *raw = PushType(req->memory.arena, Http_Header_Raw);
	if (raw) {
		raw->name        = name;
		raw->value       = value;
		raw->next        = req->headers.raw;
		req->headers.raw = raw;
		return true;
	}
	return false;
}

struct Http_Status {
	ptrdiff_t code;
	String    name;
};

struct Http_Response {
	Http_Status      status;
	Http_Header      headers;
	Buffer           body;
	Temporary_Memory memory;
};

Http_Response *Http_CreateResponse(Memory_Arena *arena) {
	Temporary_Memory memory = BeginTemporaryMemory(arena);
	Http_Response *res      = PushTypeZero(arena, Http_Response);
	res->memory             = memory;
	return res;
}

void Http_DestroyResponse(Http_Response *req) {
	EndTemporaryMemory(&req->memory);
}

Memory_Arena *Http_ResponseGetArena(Http_Response *res) {
	return res->memory.arena;
}

bool Http_ResponseAddCustomHeader(Http_Response *res, String name, String value) {
	Http_Header_Raw *raw = PushType(res->memory.arena, Http_Header_Raw);
	if (raw) {
		raw->name        = name;
		raw->value       = value;
		raw->next        = res->headers.raw;
		res->headers.raw = raw;
		return true;
	}
	return false;
}

static constexpr int HTTP_MAX_HEADER_SIZE = KiloBytes(8);

static inline bool Http_WriteBytes(Net_Socket *http, uint8_t *bytes, ptrdiff_t bytes_to_write) {
	while (bytes_to_write > 0) {
		int bytes_sent = Net_Write(http, bytes, (int)bytes_to_write);
		if (bytes_sent > 0) {
			bytes += bytes_sent;
			bytes_to_write -= bytes_sent;
			continue;
		}
		return false;
	}
	return true;
}

static inline bool Http_ReadBytes(Net_Socket *http, uint8_t *buffer, ptrdiff_t bytes_to_read) {
	while (bytes_to_read) {
		int bytes_read = Net_Read(http, buffer, (int)bytes_to_read);
		if (bytes_read > 0) {
			bytes_to_read -= bytes_read;
			buffer += bytes_read;
			continue;
		}
		return false;
	}
	return true;
}

Http_Response *Http_SendCustomMethod(Net_Socket *http, const String method, const String endpoint, Http_Request *req, Http_Body_Writer *writer = nullptr) {
	Memory_Arena *arena = Http_RequestGetArena(req);

	{
		// Send Request
		Defer{ Http_DestroyRequest(req); };

		void *mem = PushSize(arena, HTTP_MAX_HEADER_SIZE);
		if (!mem) {
			WriteLogErrorEx("Http", "Send request failed: Out of Memory");
			return nullptr;
		}

		Builder builder;
		BuilderBegin(&builder, mem, HTTP_MAX_HEADER_SIZE);
		BuilderWrite(&builder, method, String(" "), endpoint, String(" HTTP/1.1\r\n"));

		for (int id = 0; id < _HTTP_HEADER_COUNT; ++id) {
			String value = req->headers.known[id];
			if (value.length) {
				BuilderWrite(&builder, HttpHeaderMap[id], String(":"), value, String("\r\n"));
			}
		}

		for (auto raw = req->headers.raw; raw; raw = raw->next)
			BuilderWrite(&builder, raw->name, String(":"), raw->value, String("\r\n"));

		BuilderWrite(&builder, "\r\n");

		if (builder.thrown)
			return nullptr;

		String buffer = BuilderEnd(&builder);
		if (!Http_WriteBytes(http, buffer.data, buffer.length))
			return nullptr;

		if (req->body.type == Http_Request::Body::STATIC) {
			if (!Http_WriteBytes(http, req->body.data.buffer.data, req->body.data.buffer.length))
				return nullptr;
		} else {
			Http_Body_Reader reader = req->body.data.reader;
			while (true) {
				Buffer buffer = reader.proc(reader.context);
				if (!buffer.length) break;
				if (!Http_WriteBytes(http, buffer.data, buffer.length))
					return nullptr;
			}
		}
	}

	Http_Response *res = Http_CreateResponse(arena);

	const auto ResponseFailed = [res](const char *message = nullptr) -> Http_Response *{
		if (message) WriteLogErrorEx("Http", message);
		Http_DestroyResponse(res);
		return nullptr;
	};

	uint8_t *head_ptr   = nullptr;
	ptrdiff_t head_len  = 0;

	uint8_t *body_ptr   = nullptr;
	ptrdiff_t body_read = 0;

	{
		// Read Header
		uint8_t *header_mem = (uint8_t *)PushSize(arena, HTTP_MAX_HEADER_SIZE);
		if (!header_mem)
			return ResponseFailed("Reading header failed: Out of Memory");

		uint8_t *read_ptr = header_mem;
		int buffer_size = HTTP_MAX_HEADER_SIZE;
		uint8_t *parse_ptr = read_ptr;

		bool read_more = true;

		while (read_more) {
			int bytes_read = Net_Read(http, read_ptr, buffer_size);
			if (bytes_read <= 0) return ResponseFailed();

			read_ptr += bytes_read;
			buffer_size -= bytes_read;

			while (read_ptr - parse_ptr > 1) {
				String part(parse_ptr, read_ptr - parse_ptr);
				ptrdiff_t pos = StrFind(part, "\r\n");
				if (pos < 0) {
					if (buffer_size)
						break;
					return ResponseFailed("Reading header failed: Out of Memory");
				}

				parse_ptr += pos + 2;

				if (pos == 0) {
					head_ptr  = header_mem;
					head_len  = parse_ptr - header_mem;
					body_ptr  = parse_ptr;
					body_read = read_ptr - parse_ptr;
					read_more = false;

					if (buffer_size)
						PopSize(arena, buffer_size);

					break;
				}
			}
		}
	}

	String header(head_ptr, head_len);

	// Body: Content-Length
	ptrdiff_t name_pos = StrFind(header, HttpHeaderMap[HTTP_HEADER_CONTENT_LENGTH]);
	if (name_pos >= 0) {
		ptrdiff_t colon = StrFindCharacter(header, ':', name_pos);
		ptrdiff_t end   = StrFindCharacter(header, '\n', name_pos);
		if (colon < 0 || end < 0)
			return ResponseFailed("Invalid Content-Length in the header");

		String content_length_str = SubStr(header, colon + 1, end - colon - 1);
		content_length_str        = StrTrim(content_length_str);

		ptrdiff_t content_length = 0;
		if (!ParseInt(content_length_str, &content_length))
			return ResponseFailed("Invalid Content-Length in the header");

		if (content_length < body_read)
			return ResponseFailed("Invalid Content-Length in the header");

		if (writer == nullptr) {
			uint8_t *read_ptr = nullptr;
			if (body_ptr + body_read == MemoryArenaGetCurrent(arena)) {
				// Since header and body are allocated using the same arena
				// at this point, the allocation should be serial.
				// But in case we decide to change the flow of the code later
				// this might not be true, thus check is added here to ensure
				// serial and copy is the allocation is serial
				read_ptr = (uint8_t *)PushSize(arena, content_length - body_read);
			} else {
				read_ptr = (uint8_t *)PushSize(arena, content_length);
				if (read_ptr) {
					memcpy(read_ptr, body_ptr, body_read);
					body_ptr = read_ptr;
					read_ptr += body_read;
				}
			}

			if (!read_ptr)
				return ResponseFailed("Reading header failed: Out of Memory");

			if (!Http_ReadBytes(http, read_ptr, content_length - body_read))
				return ResponseFailed();
			res->body = Buffer(body_ptr, content_length);
		} else {
			Assert(body_read < HTTP_MAX_HEADER_SIZE);

			writer->proc(body_ptr, (int)body_read, writer->context);

			uint8_t *read_ptr = nullptr;
			if (body_ptr + body_read == MemoryArenaGetCurrent(arena))
				read_ptr = (uint8_t *)PushSize(arena, HTTP_MAX_HEADER_SIZE - body_read);
			else
				read_ptr = (uint8_t *)PushSize(arena, HTTP_MAX_HEADER_SIZE);

			if (!read_ptr)
				return ResponseFailed("Reading header failed: Out of Memory");

			ptrdiff_t remaining = content_length - body_read;
			while (remaining) {
				int bytes_read = Net_Read(http, read_ptr, (int)Minimum(remaining, HTTP_MAX_HEADER_SIZE));
				if (bytes_read <= 0) ResponseFailed();
				writer->proc(read_ptr, bytes_read, writer->context);
				remaining -= bytes_read;
			}

			PopSize(arena, HTTP_MAX_HEADER_SIZE);
		}
	} else {
		// Transfer-Encoding: chunked
		name_pos = StrFind(header, HttpHeaderMap[HTTP_HEADER_TRANSFER_ENCODING]);
		if (name_pos < 0)
			return ResponseFailed("No Content-Length in the header");

		ptrdiff_t colon = StrFindCharacter(header, ':', name_pos);
		ptrdiff_t end   = StrFindCharacter(header, '\n', name_pos);
		if (colon < 0 || end < 0)
			return ResponseFailed("Invalid header received");

		String value = SubStr(header, colon + 1, end - colon - 1);
		value        = StrTrim(value);

		if (StrFind(value, "chunked") >= 0) {
			uint8_t streaming_buffer[HTTP_MAX_HEADER_SIZE];
			Assert(body_read < sizeof(streaming_buffer));

			memcpy(streaming_buffer, body_ptr, body_read);

			if (body_ptr + body_read == MemoryArenaGetCurrent(arena))
				PopSize(arena, body_read);

			body_ptr = (uint8_t *)MemoryArenaGetCurrent(arena);
			uint8_t *last_ptr = body_ptr;

			ptrdiff_t chunk_read = body_read;
			while (true) {
				while (chunk_read < 2) {
					int bytes_read = Net_Read(http, streaming_buffer + chunk_read, (int)(sizeof(streaming_buffer) - chunk_read));
					if (bytes_read <= 0) return ResponseFailed();
					chunk_read += bytes_read;
				}

				String chunk_header(streaming_buffer, chunk_read);
				ptrdiff_t data_pos = StrFind(chunk_header, "\r\n");
				if (data_pos < 0)
					ResponseFailed("Reading header failed: chuck size not found");

				String length_str = SubStr(chunk_header, 0, data_pos);
				ptrdiff_t length;
				if (!ParseHex(length_str, &length) || length < 0)
					ResponseFailed("Reading header failed: Invalid chuck size");

				if (length == 0)
					break;

				Assert(last_ptr == MemoryArenaGetCurrent(arena));
				uint8_t *dst_ptr = (uint8_t *)PushSize(arena, length);
				if (!dst_ptr)
					ResponseFailed("Reading header failed: Out of memory");
				last_ptr = dst_ptr + length;

				data_pos += 2; // skip \r\n
				ptrdiff_t streamed_chunk_len = chunk_read - data_pos;
				if (length <= streamed_chunk_len) {
					memcpy(dst_ptr, streaming_buffer + data_pos, length);
					chunk_read -= (data_pos + length);
					memmove(streaming_buffer, streaming_buffer + data_pos + length, chunk_read);
				} else {
					memcpy(dst_ptr, streaming_buffer + data_pos, streamed_chunk_len);
					if (!Http_ReadBytes(http, dst_ptr + streamed_chunk_len, length - streamed_chunk_len))
						return ResponseFailed();
					chunk_read = 0;
				}

				// Reset the buffer if the writer is present
				if (writer) {
					writer->proc(dst_ptr, (int)length, writer->context);
					PopSize(arena, length);
					last_ptr = dst_ptr;
				}
			}
			if (writer == nullptr)
				res->body = Buffer(body_ptr, last_ptr - body_ptr);
		}
	}

	{
		// Parse headers
		uint8_t *trav = header.data;
		uint8_t *last = trav + header.length;

		enum { PARSISNG_STATUS, PARSING_FIELDS };

		int state = PARSISNG_STATUS;

		while (true) {
			String part(trav, last - trav);
			ptrdiff_t pos = StrFind(part, "\r\n");
			if (pos == 0) // Finished
				break;

			if (pos < 0)
				return ResponseFailed("Invalid header received");

			String line(trav, pos);
			trav += pos + 2;

			if (state == PARSING_FIELDS) {
				ptrdiff_t colon = StrFindCharacter(line, ':');
				if (colon <= 0) return ResponseFailed("Invalid header received");

				String name = SubStr(line, 0, colon);
				name = StrTrim(name);
				String value = SubStr(line, colon + 1);
				value = StrTrim(value);

				bool known_header = false;
				for (int index = 0; index < _HTTP_HEADER_COUNT; ++index) {
					if (name == HttpHeaderMap[index]) {
						res->headers.known[index] = value;
						known_header = true;
						break;
					}
				}

				if (!known_header) {
					if (!Http_ResponseAddCustomHeader(res, name, value))
						return ResponseFailed("Reading header failed: Out of Memory");
				}
			} else {
				const String prefixes[] = { "HTTP/1.1 ", "HTTP/1.0" };

				bool prefix_present = false;
				for (auto prefix : prefixes) {
					if (StrStartsWith(line, prefix)) {
						line = StrRemovePrefix(line, prefix.length);
						prefix_present = true;
						break;
					}
				}
				if (!prefix_present)
					return ResponseFailed("Invalid header received");

				line = StrTrim(line);
				ptrdiff_t name_pos = StrFindCharacter(line, ' ');
				if (name_pos < 0)
					return ResponseFailed("Invalid header received");

				if (!ParseInt(SubStr(line, 0, name_pos), &res->status.code))
					return ResponseFailed("Invalid header received");
				else if (res->status.code < 0)
					return ResponseFailed("Invalid status code received");
				res->status.name = SubStr(line, name_pos + 1);

				state = PARSING_FIELDS;
			}
		}
	}

	return res;
}

Http_Response *Http_Get(Net_Socket *http, const String endpoint, Http_Request *req, Http_Body_Writer *writer = nullptr) {
	return Http_SendCustomMethod(http, "GET", endpoint, req, writer);
}

Http_Response *Http_Post(Net_Socket *http, const String endpoint, Http_Request *req, Http_Body_Writer *writer = nullptr) {
	return Http_SendCustomMethod(http, "POST", endpoint, req, writer);
}

Http_Response *Http_Put(Net_Socket *http, const String endpoint, Http_Request *req, Http_Body_Writer *writer = nullptr) {
	return Http_SendCustomMethod(http, "PUT", endpoint, req, writer);
}

void Dump(uint8_t *buffer, int length, void *content) {
	printf("%.*s", length, buffer);
}

int main(int argc, char **argv) {
	InitThreadContext(KiloBytes(64));
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	Memory_Arena *arena = MemoryArenaAllocate(MegaBytes(64));

	Net_Socket *http = Http_Connect("https://discord.com");
	if (!http) {
		return 1;
	}

	const String token = FmtStr(arena, "Bot %s", argv[1]);

	Http_Request *req = Http_CreateRequest(http, arena);
	Http_RequestSet(req, HTTP_HEADER_AUTHORIZATION, token);
	Http_RequestSet(req, HTTP_HEADER_USER_AGENT, "Katachi");
	Http_RequestSet(req, HTTP_HEADER_CONNECTION, "keep-alive");
	Http_RequestSetContent(req, "application/json", Message);

	Http_Body_Writer writer = { Dump };

	Http_Response *res = Http_Post(http, "/api/v9/channels/850062383266136065/messages", req, &writer);
	
	if (res) {
		//printf("%.*s\n\n", (int)res->body.length, res->body.data);

		Http_DestroyResponse(res);
	}

	/*

	
	*/


#if 0
	Net_Socket *net = Net_OpenConnection("discord.com", "443", NET_SOCKET_TCP);
	if (!net) {
		return 1;
	}

	Net_OpenSecureChannel(net, false);


	String_Builder builder;

	String method = "POST";
	String endpoint = "/api/v9/channels/850062383266136065/messages";

	WriteFormatted(&builder, "% % HTTP/1.1\r\n", method, endpoint);
	WriteFormatted(&builder, "Authorization: Bot %\r\n", token);
	Write(&builder, "User-Agent: KatachiBot\r\n");
	Write(&builder, "Connection: keep-alive\r\n");
	WriteFormatted(&builder, "Host: %\r\n", Net_GetHostname(net));
	WriteFormatted(&builder, "Content-Type: %\r\n", "application/json");
	WriteFormatted(&builder, "Content-Length: %\r\n", Message.length);
	Write(&builder, "\r\n");
	WriteBuffer(&builder, Message.data, Message.length);

	// Send header
	int bytes_sent = 0;
	for (auto buk = &builder.head; buk; buk = buk->next) {
		bytes_sent += Net_Write(net, buk->data, (int)buk->written);
	}

	static char buffer[4096 * 2];

	int bytes_received = 0;
	bytes_received += Net_Read(net, buffer, sizeof(buffer) - 1);
	bytes_received += Net_Read(net, buffer + bytes_received, sizeof(buffer) - 1 - bytes_received);
	if (bytes_received < 1) {
		Unimplemented();
	}

	buffer[bytes_received] = 0;

	printf("\n%s\n", buffer);


	Net_CloseConnection(net);
#endif

	Net_Shutdown();

	return 0;
}
