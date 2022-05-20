#include "Http.h"
#include "Kr/KrString.h"

//
//
//

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

struct Url {
	String scheme;
	String host;
	String port;
};

static bool Http_UrlExtract(String hostname, Url *url) {
	ptrdiff_t host = StrFind(hostname, "://", 0);
	if (host >= 0) {
		url->scheme = SubStr(hostname, 0, host);
		ptrdiff_t colon = StrFind(hostname, ":", host + 1);
		if (colon >= 0) {
			url->host = SubStr(hostname, host + 3, colon - host - 3);
			url->port = SubStr(hostname, colon + 1);
		} else {
			url->host = SubStr(hostname, host + 3);
			url->port = url->scheme;
		}
	} else if (hostname.length) {
		url->host = hostname;
		url->scheme = "http";
		url->port = url->scheme;
	} else {
		return false;
	}
	return true;
}

//
//
//

struct Http { int __unused; };

Net_Socket *Http_GetSocket(Http *http)    { return (Net_Socket *)http; }
Http *Http_FromSocket(Net_Socket *socket) { return (Http *)socket; }

Http *Http_Connect(const String host, const String port, Http_Connection connection, Memory_Allocator allocator) {
	Net_Socket *http = Net_OpenConnection(host, port, NET_SOCKET_TCP, allocator);
	if (http) {
		if (connection == HTTP_DEFAULT && (port == "80" || StrMatchICase(port, "http"))) {
			connection = HTTP_CONNECTION;
		} else {
			connection = HTTPS_CONNECTION;
		}

		if (connection == HTTPS_CONNECTION) {
			if (Net_OpenSecureChannel(http, true)) {
				Net_SetSocketBlockingMode(http, false);
				return (Http *)http;
			}
			Net_CloseConnection(http);
			return nullptr;
		}
		Net_SetSocketBlockingMode(http, false);
		return (Http *)http;
	}
	return nullptr;
}

Http *Http_Connect(const String hostname, Http_Connection connection, Memory_Allocator allocator) {
	Url url;
	if (Http_UrlExtract(hostname, &url)) {
		if (connection == HTTP_DEFAULT && (url.scheme == "80" || StrMatchICase(url.scheme, "http")))
			connection = HTTP_CONNECTION;
		return Http_Connect(url.host, url.port, connection, allocator);
	}
	LogErrorEx("Http", "Invalid hostname: %*.s", (int)hostname.length, hostname.data);
	return nullptr;
}

bool Http_Reconnect(Http *http) {
	return Net_TryReconnect((Net_Socket *)http);
}

void Http_Disconnect(Http *http) {
	Net_CloseConnection((Net_Socket *)http);
}

//
//
//

void Http_QueryParamSet(Http_Query_Params *params, String name, String value) {
	Assert(params->count < HTTP_MAX_QUERY_PARAMS);
	Http_Query *query = &params->queries[params->count++];
	query->name  = name;
	query->value = value;
}

String Http_QueryParamGet(Http_Query_Params *params, String name) {
	for (ptrdiff_t index = 0; index < HTTP_MAX_QUERY_PARAMS; ++index) {
		if (StrMatchICase(name, params->queries[index].name))
			return params->queries[index].value;
	}
	return String();
}

void Http_DumpHeader(const Http_Request &req) {
	LogInfoEx("Http", "================== Header Dump ==================");
	LogInfo("%s ", (req.version == HTTP_VERSION_1_0 ? "HTTP/1.0" : "HTTP/1.1"));
	for (int id = 0; id < _HTTP_HEADER_COUNT; ++id) {
		if (req.headers.known[id].length)
			LogInfo("> " StrFmt ": " StrFmt, StrArg(HttpHeaderMap[id]), StrArg(req.headers.known[id]));
	}
	for (int index = 0; index < req.headers.raw.count; ++index) {
		const auto &raw = req.headers.raw.data[index];
		if (raw.name.length)
			LogInfo("> " StrFmt ": " StrFmt, StrArg(raw.name), StrArg(raw.value));
	}
	LogInfoEx("Http", "=================================================");
}

void Http_DumpHeader(const Http_Response &res) {
	const char *version = (res.status.version == HTTP_VERSION_1_0 ? "HTTP/1.0" : "HTTP/1.1");

	LogInfoEx("Http", "================== Header Dump ==================");
	LogInfo("%s %u " StrFmt, version, res.status.code, StrArg(res.status.name));
	for (int id = 0; id < _HTTP_HEADER_COUNT; ++id) {
		if (res.headers.known[id].length)
			LogInfo("> " StrFmt ": " StrFmt, StrArg(HttpHeaderMap[id]), StrArg(res.headers.known[id]));
	}
	for (int index = 0; index < res.headers.raw.count; ++index) {
		const auto &raw = res.headers.raw.data[index];
		if (raw.name.length)
			LogInfo("> " StrFmt ": " StrFmt, StrArg(raw.name), StrArg(raw.value));
	}
	LogInfoEx("Http", "=================================================");
}

void Http_InitRequest(Http_Request *req) {
	memset(req, 0, sizeof(*req));
}

void Http_SetHost(Http_Request *req, Http *http) {
	req->headers.known[HTTP_HEADER_HOST] = Net_GetHostname((Net_Socket *)http);
}

void Http_SetHeaderFmt(Http_Request *req, Http_Header_Id id, const char *fmt, ...) {
	uint8_t *buff = req->buffer + req->length;
	va_list arg;
	va_start(arg, fmt);
	int len = vsnprintf((char *)buff, sizeof(req->buffer) - req->length, fmt, arg);
	va_end(arg);
	req->length += len;
	Http_SetHeader(req, id, String(buff, len));
}

void Http_SetHeaderFmt(Http_Request *req, String name, const char *fmt, ...) {
	uint8_t *buff = req->buffer + req->length;
	va_list arg;
	va_start(arg, fmt);
	int len = vsnprintf((char *)buff, sizeof(req->buffer) - req->length, fmt, arg);
	va_end(arg);
	req->length += len;
	Http_SetHeader(req, name, String(buff, len));
}

void Http_SetHeader(Http_Request *req, Http_Header_Id id, String value) {
	req->headers.known[id] = value;
}

void Http_SetHeader(Http_Request *req, String name, String value) {
	ptrdiff_t count = req->headers.raw.count;
	Assert(count < HTTP_MAX_RAW_HEADERS);
	req->headers.raw.data[count].name = name;
	req->headers.raw.data[count].value = value;
	req->headers.raw.count += 1;
}

void Http_AppendHeader(Http_Request *req, Http_Header_Id id, String value) {
	if (!req->headers.known[id].length) {
		Http_SetHeader(req, id, value);
	} else {
		String before = req->headers.known[id];
		int written = snprintf((char *)req->buffer + req->length, HTTP_MAX_HEADER_SIZE - req->length, "%.*s,%.*s",
			StrArg(before), StrArg(value));
		req->headers.known[id] = String(req->buffer + req->length, written);
		req->length += written;
	}
}

void Http_AppendHeader(Http_Request *req, String name, String value) {
	for (ptrdiff_t index = 0; index < req->headers.raw.count; ++index) {
		if (StrMatchICase(req->headers.raw.data[index].name, name)) {
			String before = req->headers.raw.data[index].value;
			int written = snprintf((char *)req->buffer + req->length, HTTP_MAX_HEADER_SIZE - req->length, "%.*s,%.*s",
				StrArg(before), StrArg(value));
			req->headers.raw.data[index].value = String(req->buffer + req->length, written);
			req->length += written;
			return;
		}
	}
	Http_SetHeader(req, name, value);
}

void Http_SetContentLength(Http_Request *req, ptrdiff_t length) {
	if (length >= 0) {
		int written = snprintf((char *)req->buffer + req->length, HTTP_MAX_HEADER_SIZE - req->length, "%zd", length);
		String content_len(req->buffer + req->length, written);
		req->length += written;
		req->headers.known[HTTP_HEADER_CONTENT_LENGTH] = content_len;
	} else {
		req->headers.known[HTTP_HEADER_CONTENT_LENGTH] = "";
	}
}

void Http_SetContent(Http_Request *req, String type, Buffer content) {
	Http_SetContentLength(req, content.length);
	req->headers.known[HTTP_HEADER_CONTENT_TYPE] = type;
	req->body = content;
}

void Http_SetBody(Http_Request *req, Buffer content) {
	req->body = content;
}

String Http_GetHeader(Http_Request *req, Http_Header_Id id) {
	return req->headers.known[id];
}

String Http_GetHeader(Http_Request *req, const String name) {
	ptrdiff_t count = req->headers.raw.count;
	for (ptrdiff_t index = 0; index < count; ++index) {
		if (StrMatchICase(name, req->headers.raw.data[index].name)) {
			return req->headers.raw.data[index].value;
		}
	}
	return String();
}

void Http_InitResponse(Http_Response *res) {
	memset(res, 0, sizeof(*res));
}

void Http_SetHeaderFmt(Http_Response *res, Http_Header_Id id, const char *fmt, ...) {
	uint8_t *buff = res->buffer + res->length;
	va_list arg;
	va_start(arg, fmt);
	int len = vsnprintf((char *)buff, sizeof(res->buffer) - res->length, fmt, arg);
	va_end(arg);
	res->length += len;
	Http_SetHeader(res, id, String(buff, len));
}

void Http_SetHeaderFmt(Http_Response *res, String name, const char *fmt, ...) {
	uint8_t *buff = res->buffer + res->length;
	va_list arg;
	va_start(arg, fmt);
	int len = vsnprintf((char *)buff, sizeof(res->buffer) - res->length, fmt, arg);
	va_end(arg);
	res->length += len;
	Http_SetHeader(res, name, String(buff, len));
}

void Http_SetHeader(Http_Response *res, Http_Header_Id id, String value) {
	res->headers.known[id] = value;
}

void Http_SetHeader(Http_Response *res, String name, String value) {
	ptrdiff_t count = res->headers.raw.count;
	Assert(count < HTTP_MAX_RAW_HEADERS);
	res->headers.raw.data[count].name = name;
	res->headers.raw.data[count].value = value;
	res->headers.raw.count += 1;
}

void Http_AppendHeader(Http_Response *res, Http_Header_Id id, String value) {
	if (!res->headers.known[id].length) {
		Http_SetHeader(res, id, value);
	} else {
		String before = res->headers.known[id];
		int written = snprintf((char *)res->buffer + res->length, HTTP_MAX_HEADER_SIZE - res->length, "%.*s,%.*s",
			StrArg(before), StrArg(value));
		res->headers.known[id] = String(res->buffer + res->length, written);
		res->length += written;
	}
}

void Http_AppendHeader(Http_Response *res, String name, String value) {
	for (ptrdiff_t index = 0; index < res->headers.raw.count; ++index) {
		if (StrMatchICase(res->headers.raw.data[index].name, name)) {
			String before = res->headers.raw.data[index].value;
			int written = snprintf((char *)res->buffer + res->length, HTTP_MAX_HEADER_SIZE - res->length, "%.*s,%.*s",
				StrArg(before), StrArg(value));
			res->headers.raw.data[index].value = String(res->buffer + res->length, written);
			res->length += written;
			return;
		}
	}
	Http_SetHeader(res, name, value);
}

void Http_SetContentLength(Http_Response *res, ptrdiff_t length) {
	if (length >= 0) {
		int written = snprintf((char *)res->buffer + res->length, HTTP_MAX_HEADER_SIZE - res->length, "%zd", length);
		String content_len(res->buffer + res->length, written);
		res->length += written;
		res->headers.known[HTTP_HEADER_CONTENT_LENGTH] = content_len;
	} else {
		res->headers.known[HTTP_HEADER_CONTENT_LENGTH] = "";
	}
}

void Http_SetContent(Http_Response *res, String type, Buffer content) {
	Http_SetContentLength(res, content.length);
	res->headers.known[HTTP_HEADER_CONTENT_TYPE] = type;
	res->body = content;
}

void Http_SetBody(Http_Response *res, Buffer content) {
	res->body = content;
}

String Http_GetHeader(Http_Response *res, Http_Header_Id id) {
	return res->headers.known[id];
}

String Http_GetHeader(Http_Response *res, const String name) {
	ptrdiff_t count = res->headers.raw.count;
	for (ptrdiff_t index = 0; index < count; ++index) {
		if (StrMatchICase(name, res->headers.raw.data[index].name)) {
			return res->headers.raw.data[index].value;
		}
	}
	return String();
}

//
//
//


constexpr int HTTP_TIMEOUT_MS = 5000;

static inline int Http_Send(Http *http, uint8_t *bytes, int length) {
	int ret = Net_SendBlocked((Net_Socket *)http, bytes, length, HTTP_TIMEOUT_MS);
	if (ret >= 0) return ret;
	Net_Error error = Net_GetLastError((Net_Socket *)http);
	if (error == NET_E_TIMED_OUT) {
		LogErrorEx("Http", "Sending timed out");
		return -1;
	}
	return -1;
}

static inline bool Http_IterateSend(Http *http, uint8_t *bytes, ptrdiff_t bytes_to_write) {
	while (bytes_to_write > 0) {
		int bytes_sent = Http_Send(http, bytes, (int)bytes_to_write);
		if (bytes_sent <= 0)
			return false;
		bytes += bytes_sent;
		bytes_to_write -= bytes_sent;
	}
	return true;
}

static inline int Http_Receive(Http *http, uint8_t *buffer, int length) {
	int ret = Net_ReceiveBlocked((Net_Socket *)http, buffer, length, HTTP_TIMEOUT_MS);
	if (ret >= 0) return ret;
	Net_Error error = Net_GetLastError((Net_Socket *)http);
	if (error == NET_E_TIMED_OUT) {
		LogErrorEx("Http", "Receiving timed out");
		return -1;
	}
	return -1;
}

static inline void Http_FlushRead(Http *http, Http_Response *res) {
	while (true) {
		int bytes_read = Net_ReceiveBlocked((Net_Socket *)http, res->buffer, HTTP_MAX_HEADER_SIZE);
		if (bytes_read <= 0) break;
	}
}

void Http_DumpProc(Http_Header &header, uint8_t *buffer, ptrdiff_t length, void *context) {
	LogInfo(StrFmt, StrArg(String(buffer, length)));
}

ptrdiff_t Http_BuildRequest(const String method, const String endpoint, const Http_Query_Params *params, const Http_Request &req, uint8_t *buffer, ptrdiff_t buff_len) {
	Builder builder;
	BuilderBegin(&builder, buffer, HTTP_STREAM_CHUNK_SIZE);
	BuilderWrite(&builder, method, String(" "), endpoint);

	if (params && params->count > 0) {
		BuilderWrite(&builder, "?");

		BuilderWrite(&builder, params->queries[0].name);
		BuilderWrite(&builder, "=");
		BuilderWrite(&builder, params->queries[0].value);

		for (ptrdiff_t index = 1; index < params->count; ++index) {
			BuilderWrite(&builder, "&");
			BuilderWrite(&builder, params->queries[index].name);
			BuilderWrite(&builder, "=");
			BuilderWrite(&builder, params->queries[index].value);
		}
	}

	BuilderWrite(&builder, String(" HTTP/1.1\r\n"));

	for (int id = 0; id < _HTTP_HEADER_COUNT; ++id) {
		String value = req.headers.known[id];
		if (value.length) {
			BuilderWrite(&builder, HttpHeaderMap[id], String(":"), value, String("\r\n"));
		}
	}
	for (ptrdiff_t index = 0; index < req.headers.raw.count; ++index) {
		const Http_Raw_Headers::Header &raw = req.headers.raw.data[index];
		BuilderWrite(&builder, raw.name, String(":"), raw.value, String("\r\n"));
	}
	BuilderWrite(&builder, "\r\n");

	if (builder.thrown) {
		return -1;
	}

	String header = BuilderEnd(&builder);
	return header.length;
}

bool Http_SendRequest(Http *http, const String header, Http_Reader reader) {
	uint8_t buffer[HTTP_STREAM_CHUNK_SIZE];

	if (!Http_IterateSend(http, header.data, header.length))
		return false;

	while (true) {
		int read = reader.proc(buffer, HTTP_STREAM_CHUNK_SIZE, reader.context);
		if (!read) break;
		if (!Http_IterateSend(http, buffer, read))
			return false;
	}

	return true;
}

bool Http_ReceiveResponse(Http *http, Http_Response *res, Http_Writer writer) {
	uint8_t buffer[HTTP_STREAM_CHUNK_SIZE];

	ptrdiff_t body_read = 0;

	{
		// Read Header
		uint8_t *read_ptr  = res->buffer;
		int buffer_size    = HTTP_MAX_HEADER_SIZE;
		uint8_t *parse_ptr = read_ptr;

		bool read_more = true;

		while (read_more) {
			int bytes_read = Http_Receive(http, read_ptr, buffer_size);
			if (bytes_read <= 0)
				return false;

			Assert(res->buffer[0] != '\n');

			read_ptr += bytes_read;
			buffer_size -= bytes_read;

			while (read_ptr - parse_ptr > 1) {
				String part(parse_ptr, read_ptr - parse_ptr);
				ptrdiff_t pos = StrFind(part, "\r\n");
				if (pos < 0) {
					if (buffer_size)
						break;
					LogErrorEx("Http", "Reader header failed: out of memory");
					Http_FlushRead(http, res);
					return false;
				}

				parse_ptr += pos + 2;

				if (pos == 0) {
					res->length = parse_ptr - res->buffer;
					body_read = read_ptr - parse_ptr;
					Assert(body_read <= HTTP_STREAM_CHUNK_SIZE);
					memcpy(buffer, parse_ptr, body_read);
					read_more = false;
					break;
				}
			}
		}
	}

	{
		// Parse headers
		uint8_t *trav = res->buffer;
		uint8_t *last = trav + res->length;

		enum { PARSISNG_STATUS, PARSING_FIELDS };

		int state = PARSISNG_STATUS;

		while (true) {
			String part(trav, last - trav);
			ptrdiff_t pos = StrFind(part, "\r\n");
			if (pos == 0) // Finished
				break;

			if (pos < 0) {
				LogErrorEx("Http", "Corrupt header received: missing headers");
				Http_FlushRead(http, res);
				return false;
			}

			String line(trav, pos);
			trav += pos + 2;

			if (state == PARSING_FIELDS) {
				ptrdiff_t colon = StrFindChar(line, ':');
				if (colon <= 0) {
					LogErrorEx("Http", "Corrupt header received: value for header not present");
					Http_FlushRead(http, res);
					return false;
				}

				String name = SubStr(line, 0, colon);
				name = StrTrim(name);
				String value = SubStr(line, colon + 1);
				value = StrTrim(value);

				bool known_header = false;
				for (int index = 0; index < _HTTP_HEADER_COUNT; ++index) {
					if (StrMatchICase(name, HttpHeaderMap[index])) {
						Http_AppendHeader(res, (Http_Header_Id)index, value);
						known_header = true;
						break;
					}
				}

				if (!known_header) {
					if (res->headers.raw.count < HTTP_MAX_HEADER_SIZE) {
						Http_AppendHeader(res, name, value);
					} else {
						LogWarningEx("Http", "Custom header  \"" StrFmt "\" could not be added: out of memory", StrArg(name));
					}
				}
			} else {
				const String prefixes[] = { "HTTP/1.1 ", "HTTP/1.0 " };
				constexpr Http_Version versions[] = { HTTP_VERSION_1_1, HTTP_VERSION_1_0 };
				static_assert(ArrayCount(prefixes) == ArrayCount(versions), "");

				bool prefix_present = false;
				for (int index = 0; index < ArrayCount(prefixes); ++index) {
					String prefix = prefixes[index];
					if (StrStartsWithICase(line, prefix)) {
						line = StrRemovePrefix(line, prefix.length);
						res->status.version = versions[index];
						prefix_present = true;
						break;
					}
				}
				if (!prefix_present) {
					LogErrorEx("Http", "Corrupt header received: missing HTTP prefix: " StrFmt, StrArg(line));
					LogInfoEx("Http", "Received: " StrFmt, StrArg(String(res->buffer, res->length)));
					Http_FlushRead(http, res);
					return false;
				}

				line = StrTrim(line);
				ptrdiff_t name_pos = StrFindChar(line, ' ');
				if (name_pos < 0) {
					LogErrorEx("Http", "Corrupt header received: missing status code");
					LogInfoEx("Http", "Received: " StrFmt, StrArg(String(res->buffer, res->length)));
					Http_FlushRead(http, res);
					return false;
				}

				ptrdiff_t status_code;
				if (!ParseInt(SubStr(line, 0, name_pos), &status_code)) {
					LogErrorEx("Http", "Corrupt header received: invalid status code");
					LogInfoEx("Http", "Received: " StrFmt, StrArg(String(res->buffer, res->length)));
					Http_FlushRead(http, res);
					return false;
				} else if (status_code < 0) {
					LogErrorEx("Http", "Corrupt header received: status code is negative");
					LogInfoEx("Http", "Received: " StrFmt, StrArg(String(res->buffer, res->length)));
					Http_FlushRead(http, res);
					return false;
				}

				res->status.code = (uint32_t)status_code;
				res->status.name = SubStr(line, name_pos + 1);

				state = PARSING_FIELDS;
			}
		}
	}

	// Body: Content-Length
	const String content_length_value = res->headers.known[HTTP_HEADER_CONTENT_LENGTH];
	if (content_length_value.length) {
		ptrdiff_t content_length = 0;
		if (!ParseInt(content_length_value, &content_length)) {
			LogErrorEx("Http", "Corrupt header received: invalid content length");
			Http_FlushRead(http, res);
			return false;
		}

		if (content_length < body_read) {
			LogErrorEx("Http", "Corrupt header received: invalid content length");
			Http_FlushRead(http, res);
			return false;
		}

		if (body_read) {
			writer.proc(res->headers, buffer, body_read, writer.context);
		}

		ptrdiff_t remaining = content_length - body_read;
		while (remaining) {
			int bytes_read = Http_Receive(http, buffer, (int)Minimum(remaining, HTTP_STREAM_CHUNK_SIZE));
			if (bytes_read <= 0) return false;
			writer.proc(res->headers, buffer, bytes_read, writer.context);
			remaining -= bytes_read;
		}
	} else {
		String transfer_encoding = res->headers.known[HTTP_HEADER_TRANSFER_ENCODING];

		Trace("===> Transfer encoding");

		// Transfer-Encoding: chunked
		if (transfer_encoding.length && StrFindICase(transfer_encoding, "chunked") >= 0) {
			ptrdiff_t chunk_read  = body_read;

			while (true) {
				ptrdiff_t data_pos = -1;

				while (data_pos < 0) {
					int bytes_read = Http_Receive(http, buffer + chunk_read, (HTTP_STREAM_CHUNK_SIZE - (int)chunk_read));
					if (bytes_read <= 0) {
						LogErrorEx("Http", "Failed to receive chunked data");
						return false;
					}
					chunk_read += bytes_read;
					data_pos = StrFind(String(buffer, chunk_read), "\r\n");
				}

				while (data_pos >= 0) {
					String chunk_header(buffer, chunk_read);
					String length_str = SubStr(chunk_header, 0, data_pos);

					ptrdiff_t chunk_length;
					if (!ParseHex(length_str, &chunk_length) || chunk_length < 0) {
						LogErrorEx("Http", "Transfer-Encoding: invalid chunk size");
						Http_FlushRead(http, res);
						return false;
					}

					data_pos += 2; // skip \r\n
					ptrdiff_t streamed_chunk_len = chunk_read - data_pos;

					if (chunk_length <= streamed_chunk_len) {
						writer.proc(res->headers, buffer + data_pos, chunk_length, writer.context);
						chunk_read -= (data_pos + chunk_length);
						memmove(buffer, buffer + data_pos + chunk_length, chunk_read);
					} else {
						writer.proc(res->headers, buffer + data_pos, streamed_chunk_len, writer.context);

						ptrdiff_t remaining = chunk_length - streamed_chunk_len;
						while (remaining) {
							int bytes_read = Http_Receive(http, buffer, (int)Minimum(remaining, HTTP_STREAM_CHUNK_SIZE));
							if (bytes_read <= 0)
								return false;
							writer.proc(res->headers, buffer, bytes_read, writer.context);
							remaining -= bytes_read;
						}

						chunk_read = 0;
					}

					{
						// Read the next "\r\n"
						ptrdiff_t remaining = 2 - chunk_read;
						while (remaining > 0) {
							int bytes_read = Http_Receive(http, buffer + chunk_read, (HTTP_STREAM_CHUNK_SIZE - (int)chunk_read));
							if (bytes_read <= 0) {
								LogErrorEx("Http", "Failed to receive chunked data");
								return false;
							}
							chunk_read += bytes_read;
							remaining -= bytes_read;
						}
						if (buffer[0] == '\r' && buffer[1] == '\n') {
							memmove(buffer, buffer + 2, chunk_read - 2);
							chunk_read -= 2;
						} else {
							LogErrorEx("Http", "Invalid chunks present in the body");
							return false;
						}

						if (chunk_length == 0)
							return true;

						data_pos = StrFind(String(buffer, chunk_read), "\r\n");
					}
				}
			}
		}
	}

	return true;
}

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, Http_Writer writer) {
	uint8_t buffer[HTTP_STREAM_CHUNK_SIZE];

	{
		ptrdiff_t len = Http_BuildRequest(method, endpoint, &params, req, buffer, HTTP_STREAM_CHUNK_SIZE);
		if (len < 0) {
			LogErrorEx("Http", "Writing header failed: out of memory");
			return false;
		}

		if (!Http_SendRequest(http, String(buffer, len), reader))
			return false;
	}

	Http_InitResponse(res);

	bool received = Http_ReceiveResponse(http, res, writer);
	return received;
}

bool Http_Post(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "POST", endpoint, params, req, reader, res, writer);
}

bool Http_Get(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "GET", endpoint, params, req, reader, res, writer);
}

bool Http_Put(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "PUT", endpoint, params, req, reader, res, writer);
}

//
//
//

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, Http_Writer writer) {
	Http_Query_Params params;
	return Http_CustomMethod(http, method, endpoint, params, req, reader, res, writer);
}

bool Http_Post(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "POST", endpoint, req, reader, res, writer);
}

bool Http_Get(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "GET", endpoint, req, reader, res, writer);
}

bool Http_Put(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "PUT", endpoint, req, reader, res, writer);
}

//
//
//

struct Http_Buffer_Reader {
	ptrdiff_t written;
	ptrdiff_t length;
	uint8_t * buffer;
};

static int Http_BufferReaderProc(uint8_t *buffer, int length, void *context) {
	Http_Buffer_Reader *reader = (Http_Buffer_Reader *)context;
	int copy_len =(int)Minimum(length, reader->length - reader->written);
	memcpy(buffer, reader->buffer + reader->written, copy_len);
	reader->written += copy_len;
	return copy_len;
}

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, Http_Writer writer) {
	Http_Buffer_Reader buffer_reader;
	buffer_reader.written = 0;
	buffer_reader.length  = req.body.length;
	buffer_reader.buffer  = req.body.data;

	Http_Reader reader;
	reader.proc    = Http_BufferReaderProc;
	reader.context = &buffer_reader;
	
	bool result = Http_CustomMethod(http, method, endpoint, params, req, reader, res, writer);
	return result;
}

bool Http_Post(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "POST", endpoint, params, req, res, writer);
}

bool Http_Get(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "GET", endpoint, params, req, res, writer);
}

bool Http_Put(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "PUT", endpoint, params, req, res, writer);
}

//
//
//

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Request &req, Http_Response *res, Http_Writer writer) {
	Http_Query_Params params;
	return Http_CustomMethod(http, method, endpoint, params, req, res, writer);
}

bool Http_Post(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "POST", endpoint, req, res, writer);
}

bool Http_Get(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "GET", endpoint, req, res, writer);
}

bool Http_Put(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, Http_Writer writer) {
	return Http_CustomMethod(http, "PUT", endpoint, req, res, writer);
}

//
//
//

struct Http_Arena_Writer {
	Memory_Arena *arena;
	uint8_t *     last_pos;
	ptrdiff_t     length;
	Http *  socket;
};

static void Http_ArenaWriterProc(Http_Header &header, uint8_t *buffer, ptrdiff_t length, void *context) {
	Http_Arena_Writer *writer = (Http_Arena_Writer *)context;
	if (writer->length >= 0) {
		uint8_t *dst = (uint8_t *)PushSize(writer->arena, length);
		if (dst) {
			Assert(dst == writer->last_pos);
			memcpy(dst, buffer, length);
			writer->last_pos = dst + length;
			writer->length += length;
			return;
		}
	}
	Net_SetError((Net_Socket *)writer->socket, NET_E_OUT_OF_MEMORY);
	LogErrorEx("Http", "Receiving body failed: arena writer out of memory");
	writer->length = -1;
}

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, Memory_Arena *arena) {
	uint8_t *body = (uint8_t *)MemoryArenaGetCurrent(arena);
	auto temp     = BeginTemporaryMemory(arena);

	Http_Arena_Writer arena_writer;
	arena_writer.arena    = arena;
	arena_writer.last_pos = body;
	arena_writer.length   = 0;
	arena_writer.socket   = http;

	Http_Writer writer;
	writer.proc    = Http_ArenaWriterProc;
	writer.context = &arena_writer;

	bool result = Http_CustomMethod(http, method, endpoint, params, req, reader, res, writer);
	if (result && arena_writer.length >= 0) {
		res->body = Buffer(body, arena_writer.length);
		return true;
	}

	EndTemporaryMemory(&temp);

	return false;
}

bool Http_Post(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "POST", endpoint, params, req, reader, res, arena);
}

bool Http_Get(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "GET", endpoint, params, req, reader, res, arena);
}

bool Http_Put(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "PUT", endpoint, params, req, reader, res, arena);
}

//
//
//

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, Memory_Arena *arena) {
	Http_Query_Params params;
	return Http_CustomMethod(http, method, endpoint, params, req, reader, res, arena);
}

bool Http_Post(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "POST", endpoint, req, reader, res, arena);
}

bool Http_Get(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "GET", endpoint, req, reader, res, arena);
}

bool Http_Put(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "PUT", endpoint, req, reader, res, arena);
}

//
//
//

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, Memory_Arena *arena) {
	Http_Buffer_Reader res_body_reader;
	res_body_reader.written = 0;
	res_body_reader.length  = req.body.length;
	res_body_reader.buffer  = req.body.data;

	Http_Reader reader;
	reader.proc    = Http_BufferReaderProc;
	reader.context = &res_body_reader;

	bool result = Http_CustomMethod(http, method, endpoint, params, req, reader, res, arena);
	return result;
}

bool Http_Post(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "POST", endpoint, params, req, res, arena);
}

bool Http_Get(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "GET", endpoint, params, req, res, arena);
}

bool Http_Put(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "PUT", endpoint, params, req, res, arena);
}

//
//
//

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Request &req, Http_Response *res, Memory_Arena *arena) {
	Http_Query_Params params;
	return Http_CustomMethod(http, method, endpoint, params, req, res, arena);
}

bool Http_Post(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "POST", endpoint, req, res, arena);
}

bool Http_Get(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "GET", endpoint, req, res, arena);
}

bool Http_Put(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, Memory_Arena *arena) {
	return Http_CustomMethod(http, "PUT", endpoint, req, res, arena);
}

//
//
//

struct Http_Buffer_Writer {
	ptrdiff_t   written;
	ptrdiff_t   length;
	uint8_t *   buffer;
	Http *socket;
};

static void Http_BufferWriterProc(Http_Header &header, uint8_t *buffer, ptrdiff_t length, void *context) {
	Http_Buffer_Writer *writer = (Http_Buffer_Writer *)context;
	if (writer->written + length <= writer->length) {
		memcpy(writer->buffer + writer->written, buffer, length);
		writer->written += length;
		return;
	}
	Net_SetError((Net_Socket *)writer->socket, NET_E_OUT_OF_MEMORY);
	LogErrorEx("Http", "Receiving body failed: arena writer out of memory");
	writer->written = -1;
}

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	Http_Buffer_Writer buffer_writer;
	buffer_writer.written = 0;
	buffer_writer.length = length;
	buffer_writer.buffer = memory;
	buffer_writer.socket = http;

	Http_Writer writer;
	writer.proc = Http_BufferWriterProc;
	writer.context = &buffer_writer;

	bool result = Http_CustomMethod(http, method, endpoint, params, req, reader, res, writer);
	if (result && buffer_writer.written >= 0) {
		res->body = Buffer(memory, buffer_writer.written);
		return true;
	}
	return false;
}

bool Http_Post(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "POST", endpoint, params, req, reader, res, memory, length);
}

bool Http_Get(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "GET", endpoint, params, req, reader, res, memory, length);
}

bool Http_Put(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Reader reader, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "PUT", endpoint, params, req, reader, res, memory, length);
}

//
//
//

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	Http_Query_Params params;
	return Http_CustomMethod(http, method, endpoint, params, req, reader, res, memory, length);
}

bool Http_Post(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "POST", endpoint, req, reader, res, memory, length);
}

bool Http_Get(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "GET", endpoint, req, reader, res, memory, length);
}

bool Http_Put(Http *http, const String endpoint, const Http_Request &req, Http_Reader reader, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "PUT", endpoint, req, reader, res, memory, length);
}

//
//
//

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	Http_Buffer_Writer buffer_writer;
	buffer_writer.written = 0;
	buffer_writer.length  = length;
	buffer_writer.buffer  = memory;
	buffer_writer.socket  = http;

	Http_Writer writer;
	writer.proc    = Http_BufferWriterProc;
	writer.context = &buffer_writer;

	bool result = Http_CustomMethod(http, method, endpoint, params, req, res, writer);
	if (result && buffer_writer.written >= 0) {
		res->body = Buffer(memory, buffer_writer.written);
		return true;
	}
	return false;
}

bool Http_Post(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "POST", endpoint, params, req, res, memory, length);
}

bool Http_Get(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "GET", endpoint, params, req, res, memory, length);
}

bool Http_Put(Http *http, const String endpoint, const Http_Query_Params &params, const Http_Request &req, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "PUT", endpoint, params, req, res, memory, length);
}

//
//
//

bool Http_CustomMethod(Http *http, const String method, const String endpoint, const Http_Request &req, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	Http_Query_Params params;
	return Http_CustomMethod(http, method, endpoint, params, req, res, memory, length);
}

bool Http_Post(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "POST", endpoint, req, res, memory, length);
}

bool Http_Get(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "GET", endpoint, req, res, memory, length);
}

bool Http_Put(Http *http, const String endpoint, const Http_Request &req, Http_Response *res, uint8_t *memory, ptrdiff_t length) {
	return Http_CustomMethod(http, "PUT", endpoint, req, res, memory, length);
}
