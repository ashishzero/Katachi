#include "Websocket.h"
#include "NetworkNative.h"
#include "Kr/KrString.h"
#include "SHA1/SHA1.h"
#include "Base64.h"
#include <time.h>

struct Websocket { int __unused; };

constexpr int WEBSOCKET_MIN_READ_SIZE     = 256;
constexpr int WEBSOCKET_MIN_WRITE_SIZE    = 256;
constexpr int WEBSOCKET_WRITER_MAX_FRAMES = 2;

static_assert(IsPower2(WEBSOCKET_WRITER_MAX_FRAMES), "");

enum Websocket_Role {
	WEBSOCKET_ROLE_CLIENT,
	WEBSOCKET_ROLE_SERVER
};

struct Websocket_Frame {
	int       fin;
	int       rsv;
	int       opcode;
	int       masked;
	uint8_t   mask[4];
	int       hlen;
	Buffer    payload;
};

//
//
//

// circular buffer
// start = stop    : empty
// start = stop + 1: full
struct Websocket_Stream {
	ptrdiff_t p2size;
	uint8_t * buffer;
	ptrdiff_t start;
	ptrdiff_t stop;
};

static void Websocket_StreamInit(Websocket_Stream *stream, ptrdiff_t p2size, uint8_t *buff) {
	Assert(IsPower2(p2size));
	stream->p2size = p2size;
	stream->buffer = buff;
	stream->start  = 0;
	stream->stop   = 0;
}

static ptrdiff_t Websocket_StreamAvalilableSize(Websocket_Stream *stream) {
	const ptrdiff_t buffer_size = stream->p2size;
	ptrdiff_t size;
	if (stream->stop >= stream->start) {
		size = buffer_size - stream->stop;
		size += stream->start - 1;
	} else {
		size = stream->start - stream->stop - 1;
	}
	return size;
}

static ptrdiff_t Websocket_StreamConsumedSize(Websocket_Stream *stream) {
	const ptrdiff_t buffer_size = stream->p2size;
	ptrdiff_t size;
	if (stream->stop >= stream->start) {
		size = stream->stop - stream->start;
	} else {
		size = buffer_size - stream->start;
		size += stream->stop;
	}
	return size;
}

static ptrdiff_t Websocket_StreamWrite(Websocket_Stream *stream, uint8_t *buff, ptrdiff_t size) {
	const ptrdiff_t buffer_size = stream->p2size;

	ptrdiff_t written = 0;
	if (stream->stop >= stream->start) {
		ptrdiff_t write_size = Minimum(size, buffer_size - stream->stop);
		memcpy(stream->buffer + stream->stop, buff + written, write_size);
		written += write_size;
		stream->stop = (stream->stop + write_size) & (buffer_size - 1);
	}

	if (written != size) {
		ptrdiff_t write_size = Minimum(size - written, stream->start - stream->stop - 1);
		memcpy(stream->buffer + stream->stop, buff + written, write_size);
		written += write_size;
		stream->stop = (stream->stop + write_size) & (buffer_size - 1);
	}
	
	return written;
}

template <typename Reader>
static bool Websocket_StreamWrite(Websocket_Stream *stream, Reader reader) {
	const ptrdiff_t buffer_size = stream->p2size;

	while (true) {
		if (stream->stop >= stream->start) {
			ptrdiff_t read_size = buffer_size - stream->stop;
			ptrdiff_t read = reader(stream->buffer + stream->stop, read_size);
			if (read == 0) return true;
			if (read < 0) return false;
			stream->stop = (stream->stop + read) & (buffer_size - 1);
		} else {
			ptrdiff_t read_size = stream->start - stream->stop - 1;
			ptrdiff_t read = reader(stream->buffer + stream->stop, read_size);
			if (read == 0) return true;
			if (read < 0) return false;
			stream->stop = (stream->stop + read) & (buffer_size - 1);
		}
	}

	return true;
}

static ptrdiff_t Websocket_StreamReadNull(Websocket_Stream *stream, ptrdiff_t size) {
	const ptrdiff_t buffer_size = stream->p2size;
	ptrdiff_t read = 0;
	if (stream->start > stream->stop) {
		ptrdiff_t read_size = Minimum(size, buffer_size - stream->start);
		read += read_size;
		stream->start = (stream->start + read_size) & (buffer_size - 1);
	}
	if (read != size) {
		ptrdiff_t read_size = Minimum(size - read, stream->stop - stream->start);
		read += read_size;
		stream->start = (stream->start + read_size) & (buffer_size - 1);
	}
	return read;
}

static ptrdiff_t Websocket_StreamRead(Websocket_Stream *stream, uint8_t *buff, ptrdiff_t size) {
	const ptrdiff_t buffer_size = stream->p2size;

	ptrdiff_t read = 0;
	if (stream->start > stream->stop) {
		ptrdiff_t read_size = Minimum(size, buffer_size - stream->start);
		memcpy(buff, stream->buffer + stream->start, read_size);
		read += read_size;
		stream->start = (stream->start + read_size) & (buffer_size - 1);
	}

	if (read != size) {
		ptrdiff_t read_size = Minimum(size - read, stream->stop - stream->start);
		memcpy(buff, stream->buffer + stream->start, read_size);
		read += read_size;
		stream->start = (stream->start + read_size) & (buffer_size - 1);
	}

	return read;
}

template <typename Writer>
static bool Websocket_StreamRead(Websocket_Stream *stream, Writer writer) {
	const ptrdiff_t buffer_size = stream->p2size;

	while (true) {
		if (stream->start > stream->stop) {
			ptrdiff_t write_size = writer(stream->buffer + stream->start, buffer_size - stream->start);
			if (write_size == 0) return true;
			if (write_size < 0) return false;
			stream->start = (stream->start + write_size) & (buffer_size - 1);
		} else {
			ptrdiff_t write_size = writer(stream->buffer + stream->start, stream->stop - stream->start);
			if (write_size == 0) return true;
			if (write_size < 0) return false;
			stream->start = (stream->start + write_size) & (buffer_size - 1);
		}
	}

	return true;
}

//
//
//

struct Websocket_Buffer {
	ptrdiff_t length;
	uint8_t * data;
	ptrdiff_t cap;
};

static void Websocket_BufferInit(Websocket_Buffer *buffer, ptrdiff_t cap, uint8_t *data) {
	buffer->length = 0;
	buffer->data   = data;
	buffer->cap    = cap;
}

struct Websocket_WBuffer_List {
	ptrdiff_t written;
	ptrdiff_t length;
	uint8_t * data;

	Websocket_WBuffer_List *next;
};

static void Websocket_BufferListInit(Websocket_WBuffer_List *buffer, uint8_t *data) {
	buffer->written = 0;
	buffer->length  = 0;
	buffer->data    = data;
	buffer->next    = nullptr;
}

struct Websocket_Writer {
	Websocket_WBuffer_List * free_buffer;
	Websocket_WBuffer_List * curr_buffer;
	Websocket_WBuffer_List   control_buffer;
	ptrdiff_t               buffer_cap;
	Websocket_WBuffer_List   buffer_storage[WEBSOCKET_WRITER_MAX_FRAMES];
};

enum Websocket_Frame_Parser_State {
	PARSING_HEADER, PARSING_LEN2, PARSING_LEN8, PARSING_MASK, PARSING_PAYLOAD, PARSING_DROPPED
};

struct Websocket_Frame_Parser {
	Websocket_Frame_Parser_State state;
	Websocket_Frame              frame;
	ptrdiff_t                    payload_read;
};

struct Websocket_Reader {
	Websocket_Stream       stream;
	Websocket_Buffer       buffer;
	Websocket_Frame        frame;
	Websocket_Frame_Parser parser;
};

enum Websocket_State {
	WEBSOCKET_STATE_CONNECTED,
	WEBSOCKET_STATE_RECEIVED_CLOSE,
	WEBSOCKET_STATE_SENT_CLOSE,
	WEBSOCKET_STATE_DISCONNECTED,
};

struct Websocket_Context {
	Websocket_Role   role;
	Websocket_Reader reader;
	Websocket_Writer writer;
	int              timeout;
	Websocket_State  state;
};

//
//
//

struct Websocket_Uri {
	String scheme;
	String host;
	String port;
	String path;
	boolx  secure;
};

static bool Websocket_ParseURI(String uri, Websocket_Uri *parsed) {
	bool scheme = true;
	if (StrStartsWithICase(uri, "wss://")) {
		parsed->scheme = "wss";
		parsed->port = "443"; // default
		parsed->secure = true;
		uri = SubStr(uri, 6);
	} else if (StrStartsWithICase(uri, "ws://")) {
		parsed->scheme = "ws";
		parsed->port = "80"; // default
		parsed->secure = false;
		uri = SubStr(uri, 5);
	} else {
		// defaults
		parsed->scheme = "wss";
		parsed->port = "443";
		parsed->secure = true;
		scheme = false;
	}

	ptrdiff_t colon = StrFindChar(uri, ':');
	ptrdiff_t slash = StrFindChar(uri, '/');
	if (colon > slash && slash != -1) return false;

	if (colon >= 0) {
		if (colon == 0) return false;
		parsed->host = SubStr(uri, 0, colon);
		if (slash >= 0) {
			parsed->port = SubStr(uri, colon + 1, slash - colon - 1);
			parsed->path = SubStr(uri, slash);
		} else {
			parsed->port = SubStr(uri, colon + 1);
			parsed->path = "/";
		}

		if (!scheme) {
			if (StrMatchICase(parsed->port, "https") || parsed->port == "443")
				parsed->secure = true;
			else if (StrMatchICase(parsed->port, "http") || parsed->port == "80")
				parsed->secure = false;
		}
	} else {
		if (slash >= 0) {
			parsed->host = SubStr(uri, 0, slash);
			parsed->path = SubStr(uri, slash);
		} else {
			parsed->host = SubStr(uri, 0);
			parsed->path = "/";
		}
	}

	return true;
}

static inline uint32_t XorShift32() {
	static uint32_t XorShift32Seed = (0xffffffff & (ptrdiff_t)&XorShift32Seed);
	uint32_t x = XorShift32Seed;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	XorShift32Seed = x;
	return x;
}

static constexpr int WEBSOCKET_KEY_LENGTH        = 24;
static constexpr uint8_t WebsocketKeySalt[]      = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static constexpr int WEBSOCKET_SALT_LENGTH       = sizeof(WebsocketKeySalt) - 1;
static constexpr int WEBSOCKET_SALTED_KEY_LENGTH = WEBSOCKET_KEY_LENGTH + WEBSOCKET_SALT_LENGTH;

struct Websocket_Key { uint8_t data[WEBSOCKET_KEY_LENGTH]; };

static Websocket_Key Websocket_GenerateSecurityKey() {
	uint8_t nonce[16];
	for (auto &n : nonce)
		n = XorShift32() & 255;
	static_assert(sizeof(nonce) == 16, "");
	Websocket_Key key;
	EncodeBase64(Buffer((uint8_t *)nonce, sizeof(nonce)), key.data, sizeof(key.data));
	return key;
}

static uint32_t NextPowerOf2(uint32_t v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}

//
//
//

void Websocket_InitHeader(Websocket_Header *header) {
	memset(header, 0, sizeof(*header));
}

void Websocket_HeaderAddProcotols(Websocket_Header *header, String protocol) {
	Assert(header->protocols.count < WEBSOCKET_MAX_PROTOCOLS);
	header->protocols.data[header->protocols.count++] = protocol;
}

void Websocket_HeaderSet(Websocket_Header *header, Http_Header_Id id, String value) {
	header->headers.known[id] = value;
}

void Websocket_HeaderSet(Websocket_Header *header, String name, String value) {
	ptrdiff_t count = header->headers.raw.count;
	Assert(count < HTTP_MAX_RAW_HEADERS);
	header->headers.raw.data[count].name = name;
	header->headers.raw.data[count].value = value;
	header->headers.raw.count += 1;
}

//
//
//

void Websocket_OnMessageDump(Websocket *websocket, const Websocket_Event &event, void *user_context) {
	LogInfoEx("Websocket", "Message: " StrFmt, StrArg(event.message));
}

Websocket *Websocket_Connect(String uri, Http_Response *res, Websocket_Header *header, Websocket_Buffer_Size buffer_size, Memory_Allocator allocator) {
	Websocket_Uri websocket_uri;
	if (!Websocket_ParseURI(uri, &websocket_uri)) {
		LogErrorEx("Websocket", "Invalid websocket address: " StrFmt, StrArg(uri));
		return nullptr;
	}

	buffer_size.read  = Maximum(buffer_size.read, WEBSOCKET_MIN_READ_SIZE);
	buffer_size.write = Maximum(buffer_size.write, WEBSOCKET_MIN_WRITE_SIZE);

	buffer_size.read  = (ptrdiff_t)NextPowerOf2((uint32_t)buffer_size.read);
	buffer_size.write = (ptrdiff_t)NextPowerOf2((uint32_t)buffer_size.write);

	ptrdiff_t context_size = sizeof(Websocket_Context);
	context_size += buffer_size.read * 2;
	context_size += buffer_size.write * WEBSOCKET_WRITER_MAX_FRAMES;
	context_size += WEBSOCKET_MIN_WRITE_SIZE; // for control frame

	Net_Socket *socket = Net_OpenConnection(websocket_uri.host, websocket_uri.port, NET_SOCKET_TCP, context_size, allocator);
	if (!socket) return nullptr;

	if (websocket_uri.secure) {
		if (!Net_OpenSecureChannel(socket)) {
			Net_CloseConnection(socket);
			return nullptr;
		}
	}

	Net_SetSocketBlockingMode(socket, false);

	Http *http = Http_FromSocket(socket);

	Http_Request req;
	Http_InitRequest(&req);

	if (header) {
		memcpy(&req.headers, &header->headers, sizeof(req.headers));
	}

	Websocket_Key ws_key = Websocket_GenerateSecurityKey();

	Http_SetHost(&req, http);
	Http_SetHeader(&req, HTTP_HEADER_UPGRADE, "websocket");
	Http_SetHeader(&req, HTTP_HEADER_CONNECTION, "Upgrade");
	Http_SetHeader(&req, "Sec-WebSocket-Version", "13");
	Http_SetHeader(&req, "Sec-WebSocket-Key", String(ws_key.data, sizeof(ws_key.data)));
	Http_SetContent(&req, "", "");

	if (header) {
		int length = 0;
		uint8_t *start = req.buffer;
		if (header->protocols.count) {
			length += snprintf((char *)req.buffer + req.length, HTTP_MAX_HEADER_SIZE - req.length,
				StrFmt, StrArg(header->protocols.data[0]));
		}
		for (ptrdiff_t index = 1; index < header->protocols.count; ++index) {
			length += snprintf((char *)req.buffer + req.length, HTTP_MAX_HEADER_SIZE - req.length,
				StrFmt ",", StrArg(header->protocols.data[index]));
		}
		if (length) {
			Http_SetHeader(&req, "Sec-WebSocket-Protocol", String(start, length));
		}
	}

	if (Http_Get(http, websocket_uri.path, req, res, nullptr, 0) && res->status.code == 101) {
		if (!StrMatchICase(Http_GetHeader(res, HTTP_HEADER_UPGRADE), "websocket")) {
			LogErrorEx("websocket", "Upgrade header is not present in websocket handshake");
			Http_Disconnect(http);
			return nullptr;
		}

		if (!StrMatchICase(Http_GetHeader(res, HTTP_HEADER_CONNECTION), "upgrade")) {
			LogErrorEx("websocket", "Connection header is not present in websocket handshake");
			Http_Disconnect(http);
			return nullptr;
		}

		String accept = Http_GetHeader(res, "Sec-WebSocket-Accept");
		if (!accept.length) {
			LogErrorEx("websocket", "Sec-WebSocket-Accept header is not present in websocket handshake");
			Http_Disconnect(http);
			return nullptr;
		}

		uint8_t salted[WEBSOCKET_SALTED_KEY_LENGTH];
		memcpy(salted, ws_key.data, WEBSOCKET_KEY_LENGTH);
		memcpy(salted + WEBSOCKET_KEY_LENGTH, WebsocketKeySalt, WEBSOCKET_SALT_LENGTH);

		uint8_t digest[20];
		SHA1((char *)digest, (char *)salted, WEBSOCKET_SALTED_KEY_LENGTH);

		uint8_t buffer_base64[Base64EncodedSize(20)];
		ptrdiff_t base64_len = EncodeBase64(String(digest, sizeof(digest)), buffer_base64, sizeof(buffer_base64));
		if (!StrMatch(String(buffer_base64, base64_len), accept)) {
			LogErrorEx("Websocket", "Invalid accept key sent by the server");
			Http_Disconnect(http);
			return nullptr;
		}

		String extensions = Http_GetHeader(res, "Sec-WebSocket-Extensions");
		if (extensions.length) {
			LogErrorEx("Websocket", "Extensions are not supported");
			Http_Disconnect(http);
			return nullptr;
		}

		String protocols = Http_GetHeader(res, "Sec-WebSocket-Protocol");

		if (header) {
			Str_Tokenizer tokenizer;
			StrTokenizerInit(&tokenizer, extensions);
			while (StrTokenize(&tokenizer, ",")) {
				for (ptrdiff_t index = 0; index < header->protocols.count; ++index) {
					String prot = StrTrim(tokenizer.token);
					if (!StrMatchICase(prot, header->protocols.data[index])) {
						LogErrorEx("Websocket", "Unsupported Protocol \"" StrFmt "\" sent", StrArg(prot));
						Http_Disconnect(http);
						return nullptr;
					}
				}
			}
		} else {
			if (protocols.length) {
				LogErrorEx("Websocket", "Unsupproted protocols sent by server");
				Http_Disconnect(http);
				return nullptr;
			}
		}

		uint8_t *user              = (uint8_t *)Net_GetUserBuffer(socket);;
		Websocket_Context *context = (Websocket_Context *)user;

		user += sizeof(Websocket_Context);

		context->role    = WEBSOCKET_ROLE_CLIENT;
		context->state   = WEBSOCKET_STATE_CONNECTED;
		context->timeout = NET_TIMEOUT_MILLISECS;

		Websocket_StreamInit(&context->reader.stream, buffer_size.read, user);
		Websocket_BufferInit(&context->reader.buffer, buffer_size.read, user + buffer_size.read);
		user += 2 * buffer_size.read;

		for (int i = 0; i < WEBSOCKET_WRITER_MAX_FRAMES; ++i) {
			Websocket_BufferListInit(&context->writer.buffer_storage[i], user);
			user += buffer_size.write;
		}

		for (int i = 0; i < WEBSOCKET_WRITER_MAX_FRAMES - 1; ++i) {
			context->writer.buffer_storage[i].next = &context->writer.buffer_storage[i + 1];
		}

		context->writer.free_buffer = &context->writer.buffer_storage[0];
		context->writer.buffer_cap  = buffer_size.write;

		Websocket_BufferListInit(&context->writer.control_buffer, user);

		return (Websocket *)socket;
	}

	Http_Disconnect(http);
	return nullptr;
}

void Websocket_Disconnect(Websocket *websocket) {
	Net_CloseConnection((Net_Socket *)websocket);
}

void Websocket_SetTimeout(Websocket *websocket, int timeout) {
	Net_Socket *sock = (Net_Socket *)websocket;
	Websocket_Context *context = (Websocket_Context *)Net_GetUserBuffer(sock);
	context->timeout = timeout;
}

ptrdiff_t Websocket_GetFrameSize(Websocket *websocket, ptrdiff_t payload_len) {
	Net_Socket *sock = (Net_Socket *)websocket;
	Websocket_Context *context = (Websocket_Context *)Net_GetUserBuffer(sock);

	ptrdiff_t header_size;
	if (payload_len <= 125) {
		header_size = 2;
	} else if (payload_len <= UINT16_MAX) {
		header_size = 4;
	} else {
		header_size = 10;
	}

	bool masked = (context->role == WEBSOCKET_ROLE_CLIENT);
	if (masked)
		header_size += 4;

	return header_size + payload_len;
}

bool Websocket_IsWritable(Websocket *websocket) {
	Net_Socket *sock           = (Net_Socket *)websocket;
	Websocket_Context *context = (Websocket_Context *)Net_GetUserBuffer(sock);
	return context->writer.free_buffer != nullptr;
}

//
//
//

static void Websocket_MaskPayload(uint8_t *dst, uint8_t *src, uint64_t length, uint8_t mask[4]) {
	for (uint64_t i = 0; i < length; ++i)
		dst[i] = src[i] ^ mask[i & 3];
}

static ptrdiff_t Websocket_CreateFrame(uint8_t *dst, ptrdiff_t dst_size, Buffer payload, bool masked, int opcode) {
	uint8_t header[14]; // the last 4 bytes are for mask but is not actually used
	memset(header, 0, sizeof(header));

	header[0] |= 0x80; // FIN
	header[0] |= opcode;
	header[1] |= masked ? 0x80 : 0x00; // mask

	int header_size;

	if (payload.length <= 125) {
		header_size = 2;
		header[1] |= (uint8_t)payload.length;
	} else if (payload.length <= UINT16_MAX) {
		header_size = 4;
		header[1] |= 126;
		header[2] = ((payload.length & 0xff00) >> 8);
		header[3] = ((payload.length & 0x00ff) >> 0);
	} else {
		header_size = 10;
		header[1] |= 127;
		header[2] = (uint8_t)((payload.length & 0xff00000000000000) >> 56);
		header[3] = (uint8_t)((payload.length & 0x00ff000000000000) >> 48);
		header[4] = (uint8_t)((payload.length & 0x0000ff0000000000) >> 40);
		header[5] = (uint8_t)((payload.length & 0x000000ff00000000) >> 32);
		header[6] = (uint8_t)((payload.length & 0x00000000ff000000) >> 24);
		header[7] = (uint8_t)((payload.length & 0x0000000000ff0000) >> 16);
		header[8] = (uint8_t)((payload.length & 0x000000000000ff00) >> 8);
		header[9] = (uint8_t)((payload.length & 0x00000000000000ff) >> 0);
	}

	if (masked)
		header_size += 4;

	ptrdiff_t frame_size = header_size + payload.length;

	uint8_t *frame = dst;
	Assert(frame_size <= dst_size);

	memcpy(frame, header, header_size);

	if (masked) {
		uint32_t mask = XorShift32();
		uint8_t *mask_write = frame + header_size - 4;
		mask_write[0] = ((mask & 0xff000000) >> 24);
		mask_write[1] = ((mask & 0x00ff0000) >> 16);
		mask_write[2] = ((mask & 0x0000ff00) >> 8);
		mask_write[3] = ((mask & 0x000000ff) >> 0);

		Websocket_MaskPayload(frame + header_size, payload.data, payload.length, mask_write);
	} else {
		memcpy(frame + header_size, payload.data, payload.length);
	}

	return frame_size;
}

Websocket_Result Websocket_Send(Websocket *websocket, String raw_data, Websocket_Opcode opcode) {
	Net_Socket *sock           = (Net_Socket *)websocket;
	Websocket_Context *context = (Websocket_Context *)Net_GetUserBuffer(sock);

	Websocket_Writer &writer   = context->writer;

	if (context->state == WEBSOCKET_STATE_DISCONNECTED || context->state == WEBSOCKET_STATE_SENT_CLOSE)
		return WEBSOCKET_E_CLOSED;

	if (opcode & 0x08) {
		if (raw_data.length > 125)
			return WEBSOCKET_E_NOMEM;

		Websocket_WBuffer_List *buffer = &writer.control_buffer;

		bool masked = context->role == WEBSOCKET_ROLE_CLIENT;
		buffer->length  = Websocket_CreateFrame(buffer->data, WEBSOCKET_MIN_WRITE_SIZE, raw_data, masked, opcode);
		buffer->written = 0;

		if (opcode == WEBSOCKET_OP_CONNECTION_CLOSE) {
			if (context->state == WEBSOCKET_STATE_RECEIVED_CLOSE)
				context->state = WEBSOCKET_STATE_DISCONNECTED;
			else
				context->state = WEBSOCKET_STATE_SENT_CLOSE;
		}

		return WEBSOCKET_OK;
	} else {
		ptrdiff_t packet_size = Websocket_GetFrameSize(websocket, raw_data.length);
		if (packet_size > context->writer.buffer_cap)
			return WEBSOCKET_E_NOMEM;

		if (!writer.free_buffer)
			return WEBSOCKET_E_WAIT;

		Websocket_WBuffer_List *buffer = writer.free_buffer;
		writer.free_buffer             = buffer->next;
		buffer->next                   = nullptr;

		bool masked = context->role == WEBSOCKET_ROLE_CLIENT;
		buffer->length  = Websocket_CreateFrame(buffer->data, context->writer.buffer_cap, raw_data, masked, opcode);
		buffer->written = 0;

		if (writer.curr_buffer) {
			Websocket_WBuffer_List *parent = writer.curr_buffer;
			while (parent->next) {
				parent = parent->next;
			}
			parent->next = buffer;
		} else {
			writer.curr_buffer = buffer;
		}
	}

	return WEBSOCKET_OK;
}

Websocket_Result Websocket_SendText(Websocket *websocket, String raw_data) {
	return Websocket_Send(websocket, raw_data, WEBSOCKET_OP_TEXT_FRAME);
}

Websocket_Result Websocket_SendBinary(Websocket *websocket, String raw_data) {
	return Websocket_Send(websocket, raw_data, WEBSOCKET_OP_BINARY_FRAME);
}

void Websocket_Ping(Websocket *websocket, String raw_data) {
	Assert(raw_data.length <= 125);
	Websocket_Result result = Websocket_Send(websocket, raw_data, WEBSOCKET_OP_PING);
	Assert(result == WEBSOCKET_OK || result == WEBSOCKET_E_CLOSED);
}

void Websocket_Pong(Websocket *websocket, String raw_data) {
	Assert(raw_data.length <= 125);
	Websocket_Result result = Websocket_Send(websocket, raw_data, WEBSOCKET_OP_PONG);
	Assert(result == WEBSOCKET_OK || result == WEBSOCKET_E_CLOSED);
}

static String Websocket_CloseReasonMessage(Websocket_Close_Reason reason) {
	switch (reason) {
	case WEBSOCKET_CLOSE_NORMAL: return "Normal Closure";
	case WEBSOCKET_CLOSE_GOING_AWAY: return "Going Away";
	case WEBSOCKET_CLOSE_PROTOCOL_ERROR: return "Protocol error";
	case WEBSOCKET_CLOSE_UNSUPPORTED_DATA: return "Unsupported Data";
	case WEBSOCKET_CLOSE_NOT_RECEIVED: return "No Status Rcvd";
	case WEBSOCKET_CLOSE_ABNORMAL_CLOSURE: return "Abnormal Closure";
	case WEBSOCKET_CLOSE_INVALID_FRAME_PAYLOAD_DATA: return "Invalid frame payload data";
	case WEBSOCKET_CLOSE_POLICY_VOILATION: return "Policy Violation";
	case WEBSOCKET_CLOSE_MESSAGE_TOO_BIG: return "Message Too Big";
	case WEBSOCKET_CLOSE_MANDATORY_EXTENSION: return "Mandatory Ext.";
	case WEBSOCKET_CLOSE_INTERNAL_SERVER_ERROR: return "Internal Server Error";
	case WEBSOCKET_CLOSE_TLS_HANDSHAKE: return "TLS handshake";
	}
	return "Closure Unknown reason";
}

void Websocket_Close(Websocket *websocket, Websocket_Close_Reason reason) {
	uint8_t payload[125];

	payload[0] = ((0xff00 & reason) >> 8);
	payload[1] = 0xff & reason;

	String message = Websocket_CloseReasonMessage(reason);
	Assert(message.length <= sizeof(payload) - 2);
	memcpy(payload + 2, message.data, message.length);

	Websocket_Result result = Websocket_Send(websocket, String(payload, message.length + 2), WEBSOCKET_OP_CONNECTION_CLOSE);
	Assert(result == WEBSOCKET_OK || result == WEBSOCKET_E_CLOSED);
}

//
//
//



static inline void Websocket_ReaderResetParser(Websocket_Reader *reader) {
	memset(&reader->parser, 0, sizeof(reader->parser));
}

static inline void Websocket_ReaderResetParserControlFrame(Websocket_Reader *reader) {
	// If the currently parsed frame is control frame and was not dropped,
	// then we pop payload length from the reader buffer because the buffer
	// might contain payload from fragmented frames which need to be joined
	if ((reader->parser.frame.opcode & 0x08) && reader->parser.frame.payload.data)
		reader->buffer.length -= reader->parser.frame.payload.length;
	memset(&reader->parser, 0, sizeof(reader->parser));
}

static bool Websocket_ReaderParseFrame(Websocket_Reader *reader) {
	const ptrdiff_t buffer_size = reader->stream.p2size;

	Websocket_Frame_Parser &parser = reader->parser;

	uint8_t scratch[14];

	if (parser.state == PARSING_HEADER) {
		if (Websocket_StreamConsumedSize(&reader->stream) < 2) return false;

		Websocket_StreamRead(&reader->stream, scratch, 2);
		parser.frame.fin    = (scratch[0] & 0x80) >> 7;
		parser.frame.rsv    = (scratch[0] & 0x70) >> 4;
		parser.frame.opcode = (scratch[0] & 0x0f) >> 0;
		parser.frame.masked = (scratch[1] & 0x80) >> 7;
		int payload_len     = (scratch[1] & 0x7f);

		if (payload_len <= 125) {
			parser.frame.payload.length = payload_len;
			parser.frame.hlen           = 2;
			parser.state                = parser.frame.masked ? PARSING_MASK : PARSING_PAYLOAD;
		} else if (payload_len == 126) {
			parser.frame.hlen = 4;
			parser.state      = PARSING_LEN2;
		} else {
			parser.frame.hlen = 10;
			parser.state      = PARSING_LEN8;
		}

		if (parser.frame.masked)
			parser.frame.hlen += 4;
	}

	if (parser.state == PARSING_LEN2) {
		if (Websocket_StreamConsumedSize(&reader->stream) < 2) return false;

		Websocket_StreamRead(&reader->stream, scratch, 2);
		ptrdiff_t payload_len       = (((uint16_t)scratch[0] << 8) | (uint16_t)scratch[1]);
		parser.state                = parser.frame.masked ? PARSING_MASK : PARSING_PAYLOAD;
		parser.frame.payload.length = payload_len;
	}

	if (parser.state == PARSING_LEN8) {
		if (Websocket_StreamConsumedSize(&reader->stream) < 8) return false;

		Websocket_StreamRead(&reader->stream, scratch, 8);
		ptrdiff_t payload_len = (
			((uint64_t)scratch[0] << 56) | ((uint64_t)scratch[1] << 48) |
			((uint64_t)scratch[2] << 40) | ((uint64_t)scratch[3] << 32) |
			((uint64_t)scratch[4] << 24) | ((uint64_t)scratch[5] << 16) |
			((uint64_t)scratch[6] <<  8) | ((uint64_t)scratch[7] << 0));
		parser.state                = parser.frame.masked ? PARSING_MASK : PARSING_PAYLOAD;
		parser.frame.payload.length = payload_len;
	}

	if (parser.state == PARSING_MASK) {
		if (Websocket_StreamConsumedSize(&reader->stream) < 4) return false;

		Websocket_StreamRead(&reader->stream, parser.frame.mask, 4);
		parser.state = PARSING_PAYLOAD;
	}

	if (parser.state == PARSING_PAYLOAD) {
		Assert(parser.frame.payload.data == nullptr);

		ptrdiff_t remaining_payload = parser.frame.payload.length - parser.payload_read;

		if (remaining_payload <= (reader->buffer.cap - reader->buffer.length)) {
			ptrdiff_t read = Websocket_StreamRead(&reader->stream, reader->buffer.data + reader->buffer.length, remaining_payload);
			reader->buffer.length += read;
			parser.payload_read += read;
			remaining_payload -= read;

			if (!remaining_payload) {
				parser.frame.payload.data = reader->buffer.data;
				return true;
			}
		} else {
			reader->buffer.length -= parser.payload_read;
			parser.state = PARSING_DROPPED;
		}
	}

	if (parser.state == PARSING_DROPPED) {
		ptrdiff_t remaining_payload = parser.frame.payload.length - parser.payload_read;
		ptrdiff_t dropped = Websocket_StreamReadNull(&reader->stream, remaining_payload);
		parser.payload_read += dropped;
		remaining_payload -= dropped;
		LogWarningEx("Websocket", "Dropped %d bytes. Frame payload too big. Skipped frame", (int)dropped);

		if (!remaining_payload) {
			return true;
		}
	}

	return false;
}

static inline void Websocket_OnEvent(Websocket *websocket, Websocket_Event_Type type, Buffer message, Websocket_On_Event_Proc proc, void *context) {
	Websocket_Event event;
	event.type    = type;
	event.message = message;
	proc(websocket, event, context);
}

static void Websocket_HandleMessage(Websocket *websocket, Websocket_Context *context, Websocket_On_Event_Proc event_proc, void *event_data) {
	Websocket_Frame &frame = context->reader.parser.frame;

	if (!frame.payload.data) { // dropped frame
		Websocket_ReaderResetParserControlFrame(&context->reader);
		Websocket_Close(websocket, WEBSOCKET_CLOSE_MESSAGE_TOO_BIG);
		return;
	}

	if (frame.rsv) {
		Websocket_ReaderResetParserControlFrame(&context->reader);
		Websocket_Close(websocket, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
		return;
	}

	if (context->role == WEBSOCKET_ROLE_CLIENT && frame.masked) {
		LogErrorEx("Websocket", "Server sent masked payload. Closing...");
		Websocket_ReaderResetParserControlFrame(&context->reader);
		Websocket_Close(websocket, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
		return;
	}

	if (context->role == WEBSOCKET_ROLE_SERVER && !frame.masked) {
		LogErrorEx("Websocket", "Client sent unmasked payload. Closing...");
		Websocket_ReaderResetParserControlFrame(&context->reader);
		Websocket_Close(websocket, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
		return;
	}

	if (frame.masked) {
		Websocket_MaskPayload(frame.payload.data, frame.payload.data, frame.payload.length, frame.mask);
	}

	if (frame.opcode & 0x08) {
		// control frame
		if (!frame.fin) {
			LogErrorEx("Websocket", "Server sent fragmented control frame. Closing...");
			Websocket_ReaderResetParserControlFrame(&context->reader);
			Websocket_Close(websocket, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
			return;
		}

		if (frame.payload.length > 125) {
			LogErrorEx("Websocket", "Server sent control frame with %d bytes payload. Only upto 125 bytes is allowed. Closing...", (int)frame.payload.length);
			Websocket_ReaderResetParserControlFrame(&context->reader);
			Websocket_Close(websocket, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
			return;
		}

		switch (frame.opcode) {
		case WEBSOCKET_OP_PING: {
			Websocket_OnEvent(websocket, WEBSOCKET_EVENT_PING, frame.payload, event_proc, event_data);
			Websocket_Pong(websocket, frame.payload);
		} break;

		case WEBSOCKET_OP_PONG: {
			Websocket_OnEvent(websocket, WEBSOCKET_EVENT_PONG, frame.payload, event_proc, event_data);
		} break;

		case WEBSOCKET_OP_CONNECTION_CLOSE: {
			Websocket_OnEvent(websocket, WEBSOCKET_EVENT_CLOSE, frame.payload, event_proc, event_data);

			if (context->state == WEBSOCKET_STATE_CONNECTED) {
				Websocket_Result result = Websocket_Send(websocket, frame.payload, WEBSOCKET_OP_CONNECTION_CLOSE);
				if (result != WEBSOCKET_OK)
					LogWarningEx("Websocket", "Failed to close the remote connection. Error code: %d", result);
				context->state = WEBSOCKET_STATE_RECEIVED_CLOSE;
			} else {
				context->state = WEBSOCKET_STATE_DISCONNECTED;
			}
		} break;
		}

		Websocket_ReaderResetParserControlFrame(&context->reader);
		return;
	}

	if (frame.fin) {
		if (frame.opcode != WEBSOCKET_OP_CONTINUATION_FRAME) {
			// single frame
			Websocket_Event_Type event_type = frame.opcode == WEBSOCKET_OP_TEXT_FRAME ? WEBSOCKET_EVENT_TEXT : WEBSOCKET_EVENT_BINARY;
			Websocket_OnEvent(websocket, event_type, frame.payload, event_proc, event_data);
			context->reader.buffer.length -= frame.payload.length;
		} else {
			// final frame of fragmented frame
			Assert(context->reader.frame.payload.data + context->reader.frame.payload.length == frame.payload.data);
			context->reader.frame.payload.length += frame.payload.length;
			Websocket_Event_Type event_type = context->reader.frame.opcode == WEBSOCKET_OP_TEXT_FRAME ? WEBSOCKET_EVENT_TEXT : WEBSOCKET_EVENT_BINARY;
			Websocket_OnEvent(websocket, event_type, context->reader.frame.payload, event_proc, event_data);
			context->reader.buffer.length -= context->reader.frame.payload.length;
			memset(&context->reader.frame, 0, sizeof(context->reader.frame));
		}
	} else {
		if (context->reader.frame.opcode) {
			// continuation of fragmented frame
			if (frame.opcode) {
				LogErrorEx("Websocket", "Expected next fragment with 0x0 opcode, but got %d. Closing...", frame.opcode);
				Websocket_ReaderResetParser(&context->reader);
				Websocket_Close(websocket, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
				return;
			}

			Assert(context->reader.frame.payload.data + context->reader.frame.payload.length == frame.payload.data);
			context->reader.frame.payload.length += frame.payload.length;
		} else {
			// first part of fragmented frame
			if (!frame.opcode) {
				LogErrorEx("Websocket", "Expected next fragment with 0x0 opcode, but got %d. Closing...", frame.opcode);
				Websocket_ReaderResetParser(&context->reader);
				Websocket_Close(websocket, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
				return;
			}
			context->reader.frame = frame;
			context->reader.frame.payload = frame.payload;
		}
	}

	Websocket_ReaderResetParser(&context->reader);
}

void Websocket_Loop(Websocket *websocket, Websocket_On_Event_Proc event_proc, void *event_data) {
	Net_Socket *sock           = (Net_Socket *)websocket;
	Websocket_Context *context = (Websocket_Context *)Net_GetUserBuffer(sock);

	if (context->writer.free_buffer)
		Websocket_OnEvent(websocket, WEBSOCKET_EVENT_WRITE, "", event_proc, event_data);

	pollfd fd;
	fd.fd = Net_GetSocketDescriptor(sock);

	while (context->state != WEBSOCKET_STATE_DISCONNECTED) {
		int timeout = context->timeout;

		time_t counter = clock();

		while (timeout > 0) {
			fd.events  = 0;
			fd.revents = 0;

			if ((context->state == WEBSOCKET_STATE_CONNECTED || context->state == WEBSOCKET_STATE_RECEIVED_CLOSE) &&
				Websocket_StreamAvalilableSize(&context->reader.stream))
				fd.events |= POLLRDNORM;

			if (context->writer.curr_buffer || context->writer.control_buffer.length)
				fd.events |= POLLWRNORM;

			if (context->state == WEBSOCKET_STATE_DISCONNECTED && fd.events == 0)
				return;

			int presult = poll(&fd, 1, timeout);

			if (presult < 0) {
				LogErrorEx("Websocket", "Connection lost abrubtly");
				return;
			}

			if (presult == 0) break;

			if (fd.revents & POLLWRNORM) {
				Websocket_WBuffer_List *buffer = nullptr;

				bool is_control_frame = false;

				if (context->writer.control_buffer.length) {
					if (!context->writer.curr_buffer->written) {
						buffer           = &context->writer.control_buffer;
						is_control_frame = true;
					} else {
						buffer = context->writer.curr_buffer;
						context->writer.curr_buffer = buffer->next;
						buffer->next = nullptr;
					}
				} else {
					buffer = context->writer.curr_buffer;
					context->writer.curr_buffer = buffer->next;
					buffer->next = nullptr;
				}

				if (buffer) {
					if (!is_control_frame) {
						ptrdiff_t remaining = buffer->length - buffer->written;
						uint8_t *write_ptr  = buffer->data + buffer->written;

						while (remaining) {
							int bytes_sent = Net_Send(sock, write_ptr, (int)remaining);
							if (bytes_sent < 0) return;
							if (bytes_sent == 0) break;
							remaining -= bytes_sent;
							write_ptr += bytes_sent;
							buffer->written += bytes_sent;
						}

						if (!remaining) {
							context->writer.curr_buffer = buffer->next;
							buffer->next                = context->writer.free_buffer;
							context->writer.free_buffer = buffer;
							Websocket_OnEvent(websocket, WEBSOCKET_EVENT_WRITE, "", event_proc, event_data);
						}
					} else {
						// Send control frames all at once since they will be replaced with another control frame
						// if control frames are sent, and breaking them will cause error
						ptrdiff_t remaining = context->writer.control_buffer.length;
						uint8_t *write_ptr  = context->writer.control_buffer.data;
						while (remaining) {
							int sent = Net_SendBlocked(sock, write_ptr, (int)remaining);
							if (sent < 0) return;
							remaining -= sent;
							write_ptr += sent;
						}
					}
				}
			}

			if (fd.revents & POLLRDNORM) {
				const auto reader = [sock](uint8_t *buff, ptrdiff_t len) -> ptrdiff_t {
					return Net_Receive(sock, buff, (int)len);
				};

				if (!Websocket_StreamWrite(&context->reader.stream, reader)) {
					LogErrorEx("Websocket", "Connection lost abrubtly");
					return;
				}

				while (Websocket_ReaderParseFrame(&context->reader)) {
					Websocket_HandleMessage(websocket, context, event_proc, event_data);
				}
			}

			time_t current_counter = clock();
			int time_spent_ms = (int)((current_counter - counter) * 1000.0f / (float)CLOCKS_PER_SEC);
			counter = current_counter;

			timeout -= time_spent_ms;
		}

		Websocket_OnEvent(websocket, WEBSOCKET_EVENT_TIMEOUT, "", event_proc, event_data);
	}
}
