#include "Websocket.h"
#include "NetworkNative.h"
#include "Kr/KrString.h"
#include "Kr/KrAtomic.h"
#include "Kr/KrThread.h"
#include "Base64.h"
#include "SHA1.h"

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

//
//
//

struct Websocket { ptrdiff_t __unused; };

enum Websocket_Connection {
	WEBSOCKET_CONNECTED,
	WEBSOCKET_RECEIVED_CLOSE,
	WEBSOCKET_SENT_CLOSE,
	WEBSOCKET_DISCONNECTING,
	WEBSOCKET_DISCONNECTED,
};

enum Websocket_Role {
	WEBSOCKET_ROLE_CLIENT,
	WEBSOCKET_ROLE_SERVER
};

constexpr int WEBSOCKET_QUEUE_MIN_BUFFER_SIZE = 8;

struct Websocket_Queue {
	struct Node {
		Node *    prev;
		Node *    next;
		int32_t   header;
		ptrdiff_t len;
		uint8_t   buff[WEBSOCKET_QUEUE_MIN_BUFFER_SIZE];
	};
	Atomic_Guard    guard;
	ptrdiff_t       buffp2cap;
	Node            head;
	Node *          tail;
	Node *          free;

#if defined(WEBSOCKET_ENABLE_DEBUG_INFO)
	struct {
		int in_queue;
		int allocated;
		int free;
	} debug_info;
#endif
};

constexpr uint32_t WEBSOCKET_WRITER_CONTROL_BUFFER_SIZE = 256;
constexpr uint32_t WEBSOCKET_MIN_QUEUE_SIZE             = 16;

struct Websocket_Frame {
	int32_t   header;
	int       fin;
	int       rsv;
	int       opcode;
	int       masked;
	uint8_t   mask[4];
	int       hlen;
	Buffer    payload;
};

enum Websocket_Frame_Parser_State {
	PARSING_HEADER, PARSING_LEN2, PARSING_LEN8, PARSING_MASK, PARSING_PAYLOAD_PRECHECK, PARSING_PAYLOAD, PARSING_DROPPED
};

struct Websocket_Frame_Parser {
	Websocket_Frame_Parser_State state;
	Websocket_Frame              frame;
	ptrdiff_t                    payload_parsed;
};

struct Websocket_Read_Stream {
	ptrdiff_t start;
	ptrdiff_t stop;
	ptrdiff_t p2cap;
	uint8_t * buffer;
	ptrdiff_t seriallen;
	uint8_t * serialized;
};

struct Websocket_Reader {
	Websocket_Frame_Parser parser;
	Websocket_Queue::Node *curr_node;
	Websocket_Read_Stream  stream;
};

struct Websocket_Writer {
	struct {
		ptrdiff_t              written;
		Websocket_Queue::Node *curr_node;
	} normal;
	struct {
		ptrdiff_t length;
		uint8_t   buffer[WEBSOCKET_WRITER_CONTROL_BUFFER_SIZE];
	} control;
};

struct Websocket_Context {
	Websocket_Connection   connection;
	Websocket_Role         role;
	Websocket_Queue        readq;
	Websocket_Reader       reader;
	Semaphore *            readsem;
	Websocket_Writer       writer;
	Websocket_Queue        writeq;
	Semaphore *            writesem;
	Thread *               thread;
};

static ptrdiff_t Websocket_GetReaderSize(uint32_t p2buff_size) {
	Assert(IsPower2(p2buff_size));
	return p2buff_size * 2; // circular + serial buffer
}

static ptrdiff_t Websocket_GetQueueNodeSize(uint32_t p2buff_size) {
	Assert(IsPower2(p2buff_size));
	return sizeof(Websocket_Queue::Node) + p2buff_size - WEBSOCKET_QUEUE_MIN_BUFFER_SIZE;
}

static ptrdiff_t Websocket_GetQueueSize(uint32_t p2buff_size, uint32_t count) {
	ptrdiff_t node_size = Websocket_GetQueueNodeSize(p2buff_size);
	return count * node_size;
}

static ptrdiff_t Websocket_GetContextSize(Websocket_Spec spec) {
	ptrdiff_t size = 0;
	size += Websocket_GetReaderSize(spec.read_size);
	size += Websocket_GetQueueSize(spec.read_size, spec.queue_size);
	size += Websocket_GetQueueSize(spec.write_size, spec.queue_size);
	return size;
}

static uint8_t *Websocket_InitReader(Websocket_Reader *reader, uint32_t p2buff_size, uint8_t *mem) {
	reader->stream.p2cap      = p2buff_size;
	reader->stream.buffer     = mem;
	reader->stream.serialized = mem + p2buff_size;
	return mem + 2 * p2buff_size;
}

static uint8_t *Websocket_InitQueue(Websocket_Queue *queue, uint32_t p2buff_size, uint32_t count, uint8_t *mem) {
	queue->buffp2cap = p2buff_size;
	queue->head.next = &queue->head;
	queue->head.prev = &queue->head;
	queue->tail      = &queue->head;

	ptrdiff_t node_size = Websocket_GetQueueNodeSize(p2buff_size);

	Websocket_Queue::Node *next = (Websocket_Queue::Node *)mem;
	mem += node_size;

	Websocket_Queue::Node *node = nullptr;
	for (uint32_t index = 0; index < count - 1; ++index) {
		node = (Websocket_Queue::Node *)mem;
		mem += node_size;
		node->next = next;
		next       = node;
	}

	queue->free = next;

	return mem;
}

static void Websocket_InitContextClient(Websocket_Context *context, Websocket_Spec spec, uint8_t *mem) {
	mem = Websocket_InitReader(&context->reader, spec.read_size, mem);
	mem = Websocket_InitQueue(&context->readq, spec.read_size, spec.queue_size, mem);
	mem = Websocket_InitQueue(&context->writeq, spec.write_size, spec.queue_size, mem);

	context->connection = WEBSOCKET_CONNECTED;
	context->role       = WEBSOCKET_ROLE_CLIENT;
	context->readsem    = Semaphore_Create(0);
	context->writesem   = Semaphore_Create(spec.queue_size);
}

//
//
//

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

static int Websocket_ThreadProc(void *arg);

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

Websocket *Websocket_Connect(String uri, Http_Response *res, Websocket_Header *header, Websocket_Spec spec, Memory_Allocator allocator) {
	Websocket_Uri websocket_uri;
	if (!Websocket_ParseURI(uri, &websocket_uri)) {
		LogErrorEx("Websocket", "Invalid websocket address: " StrFmt, StrArg(uri));
		return nullptr;
	}

	spec.read_size  = Maximum(WEBSOCKET_QUEUE_MIN_BUFFER_SIZE, NextPowerOf2(spec.read_size));
	spec.write_size = Maximum(WEBSOCKET_QUEUE_MIN_BUFFER_SIZE, NextPowerOf2(spec.write_size));
	spec.queue_size = Maximum(WEBSOCKET_MIN_QUEUE_SIZE, spec.queue_size);

	ptrdiff_t context_size = sizeof(Websocket_Context) + Websocket_GetContextSize(spec);

	Net_Socket *socket = Net_OpenConnection(websocket_uri.host, websocket_uri.port, NET_SOCKET_TCP, context_size, allocator);
	if (!socket) return nullptr;

	if (websocket_uri.secure) {
		if (!Net_OpenSecureChannel(socket)) {
			LogErrorEx("Websocket", "Failed to open secure channel");
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
			LogErrorEx("Websocket", "Upgrade header is not present in websocket handshake");
			Http_Disconnect(http);
			return nullptr;
		}

		if (!StrMatchICase(Http_GetHeader(res, HTTP_HEADER_CONNECTION), "upgrade")) {
			LogErrorEx("Websocket", "Connection header is not present in websocket handshake");
			Http_Disconnect(http);
			return nullptr;
		}

		String accept = Http_GetHeader(res, "Sec-WebSocket-Accept");
		if (!accept.length) {
			LogErrorEx("Websocket", "Sec-WebSocket-Accept header is not present in websocket handshake");
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

		uint8_t *user = (uint8_t *)Net_GetUserBuffer(socket);;
		Websocket_Context *context = (Websocket_Context *)user;
		Websocket_InitContextClient(context, spec, user + sizeof(Websocket_Context));

		Thread_Context_Params params = ThreadContextDefaultParams;
		params.logger = ThreadContext.logger;

		context->thread = Thread_Create(Websocket_ThreadProc, socket, 0, params);

		return (Websocket *)socket;
	}

	Http_Disconnect(http);
	return nullptr;
}

void Websocket_Disconnect(Websocket *websocket) {
	Net_CloseConnection((Net_Socket *)websocket);
}

//
//
//

static Websocket_Queue::Node *Websocket_QueueAlloc(Websocket_Queue *q) {
	SpinLock(&q->guard);
	Websocket_Queue::Node *node = q->free;
	if (node) {
		q->free    = node->next;
		node->next = nullptr;
#if defined(WEBSOCKET_ENABLE_DEBUG_INFO)
	q->debug_info.allocated += 1;
	q->debug_info.free -= 1;
#endif
	}
	SpinUnlock(&q->guard);
	return node;
}

static void Websocket_QueueFree(Websocket_Queue *q, Websocket_Queue::Node *node) {
	SpinLock(&q->guard);
	Assert(!node->prev && !node->next);
	node->next = q->free;
	q->free    = node;
#if defined(WEBSOCKET_ENABLE_DEBUG_INFO)
	q->debug_info.allocated -= 1;
	q->debug_info.free += 1;
#endif
	SpinUnlock(&q->guard);
}

static void Websocket_QueuePush(Websocket_Queue *q, Websocket_Queue::Node *node) {
	SpinLock(&q->guard);
	Assert(!node->prev && !node->next);
	q->tail->next = node;
	node->prev = q->tail;
	node->next = &q->head;
#if defined(WEBSOCKET_ENABLE_DEBUG_INFO)
	q->debug_info.allocated -= 1;
	q->debug_info.in_queue += 1;
#endif
	SpinUnlock(&q->guard);
}

static Websocket_Queue::Node *Websocket_QueuePop(Websocket_Queue *q) {
	Websocket_Queue::Node *node = nullptr;
	SpinLock(&q->guard);
	if (q->head.next != &q->head) {
		node         = q->head.next;
		auto next    = node->next;
		q->head.next = next;
		next->prev   = &q->head;
		node->prev   = node->next = nullptr;
#if defined(WEBSOCKET_ENABLE_DEBUG_INFO)
	q->debug_info.allocated += 1;
	q->debug_info.in_queue -= 1;
#endif
	}
	SpinUnlock(&q->guard);
	return node;
}

//
//
//

static ptrdiff_t Websocket_StreamReadableSize(Websocket_Read_Stream *stream) {
	ptrdiff_t size;
	if (stream->stop >= stream->start) {
		size = stream->stop - stream->start;
	} else {
		size = stream->p2cap - stream->start;
		size += stream->stop;
	}
	return size;
}

template <typename Reader>
static bool Websocket_StreamDump(Websocket_Read_Stream *stream, Reader reader) {
	const ptrdiff_t buffer_size = stream->p2cap;
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

static ptrdiff_t Websocket_StreamDrop(Websocket_Read_Stream *stream, ptrdiff_t size) {
	ptrdiff_t read = 0;
	if (stream->start > stream->stop) {
		ptrdiff_t read_size = Minimum(size, stream->p2cap - stream->start);
		read += read_size;
		stream->start = (stream->start + read_size) & (stream->p2cap - 1);
	}
	if (read != size) {
		ptrdiff_t read_size = Minimum(size - read, stream->stop - stream->start);
		read += read_size;
		stream->start = (stream->start + read_size) & (stream->p2cap - 1);
	}
	return read;
}

static ptrdiff_t Websocket_StreamRead(Websocket_Read_Stream *stream, uint8_t *buff, ptrdiff_t size) {
	ptrdiff_t read = 0;
	if (stream->start > stream->stop) {
		ptrdiff_t read_size = Minimum(size, stream->p2cap - stream->start);
		memcpy(buff, stream->buffer + stream->start, read_size);
		read += read_size;
		stream->start = (stream->start + read_size) & (stream->p2cap - 1);
	}
	if (read != size) {
		ptrdiff_t read_size = Minimum(size - read, stream->stop - stream->start);
		memcpy(buff, stream->buffer + stream->start, read_size);
		read += read_size;
		stream->start = (stream->start + read_size) & (stream->p2cap - 1);
	}
	return read;
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

//
//
//

static void Websocket_InitReadNode(Websocket_Context *ctx) {
	if (ctx->reader.curr_node)
		ctx->reader.curr_node->header = 0;
}

static void Websocket_InspectWriteFrameForClose(Websocket_Context *ctx, int opcode) {
	if (opcode & WEBSOCKET_OP_CONNECTION_CLOSE) {
		if (ctx->connection == WEBSOCKET_CONNECTED)
			ctx->connection = WEBSOCKET_SENT_CLOSE;
		else
			ctx->connection = WEBSOCKET_DISCONNECTING;
	}
}

static bool Websocket_HasWrite(Websocket_Context *ctx) {
	if (ctx->connection != WEBSOCKET_SENT_CLOSE) {
		if (!ctx->writer.control.length) {
			if (ctx->writer.normal.curr_node)
				return true;
			ctx->writer.normal.curr_node = Websocket_QueuePop(&ctx->writeq);
			return ctx->writer.normal.curr_node;
		}
	}
	return false;
}

static bool Websocket_HasRead(Websocket_Context *ctx) {
	if (ctx->connection != WEBSOCKET_RECEIVED_CLOSE) {
		if (!ctx->reader.curr_node) {
			ctx->reader.curr_node = Websocket_QueueAlloc(&ctx->readq);
			Websocket_InitReadNode(ctx);
			return ctx->reader.curr_node;
		}
		return true;
	}
	return false;
}

//
//
//

static void Websocket_SendImmediateControlMessage(Websocket_Context *ctx, String msg, Websocket_Opcode opcode) {
	if (ctx->connection == WEBSOCKET_DISCONNECTING ||
		ctx->connection == WEBSOCKET_DISCONNECTED || 
		ctx->connection == WEBSOCKET_SENT_CLOSE)
		return;

	if (ctx->writer.control.length) {
		int opcode = ctx->writer.control.buffer[0] & 0x0f;
		if (opcode == WEBSOCKET_OP_CONNECTION_CLOSE)
			return;
	}

	Assert(msg.length <= 125);
	bool masked = ctx->role == WEBSOCKET_ROLE_CLIENT;
	ctx->writer.control.length = Websocket_CreateFrame(ctx->writer.control.buffer, 
		WEBSOCKET_WRITER_CONTROL_BUFFER_SIZE, msg, masked, opcode);
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

static void Websocket_SendImmediateClose(Websocket_Context *ctx, Websocket_Close_Reason reason) {
	uint8_t payload[125];

	payload[0] = ((0xff00 & reason) >> 8);
	payload[1] = 0xff & reason;

	String message = Websocket_CloseReasonMessage(reason);
	Assert(message.length <= sizeof(payload) - 2);
	memcpy(payload + 2, message.data, message.length);

	Websocket_SendImmediateControlMessage(ctx, String(payload, message.length + 2), WEBSOCKET_OP_CONNECTION_CLOSE);
}

static void Websocket_ImmediatePing(Websocket_Context *ctx, String message) {
	Websocket_SendImmediateControlMessage(ctx, message, WEBSOCKET_OP_PING);
}

static void Websocket_ImmediatePong(Websocket_Context *ctx, String message) {
	Websocket_SendImmediateControlMessage(ctx, message, WEBSOCKET_OP_PONG);
}

static inline void Websocket_ResetParser(Websocket_Context *ctx) {
	// Pop back the payload of the current frame
	// This will make sure that the fragmented frames are serially joined
	// in the cases where a control frames is received in between fragmented frames
	ctx->reader.stream.seriallen -= ctx->reader.parser.payload_parsed;
	memset(&ctx->reader.parser, 0, sizeof(ctx->reader.parser));
}

static bool Websocket_ParseFrame(Websocket_Context *ctx) {
	Websocket_Reader &reader       = ctx->reader;
	Websocket_Read_Stream &stream  = reader.stream;
	Websocket_Frame_Parser &parser = reader.parser;

	uint8_t scratch[14];

	if (parser.state == PARSING_HEADER) {
		if (Websocket_StreamReadableSize(&stream) < 2) return false;

		Websocket_StreamRead(&stream, scratch, 2);
		parser.frame.header = scratch[0];
		parser.frame.fin    = (scratch[0] & 0x80) >> 7;
		parser.frame.rsv    = (scratch[0] & 0x70) >> 4;
		parser.frame.opcode = (scratch[0] & 0x0f) >> 0;
		parser.frame.masked = (scratch[1] & 0x80) >> 7;
		int payload_len = (scratch[1] & 0x7f);

		if (payload_len <= 125) {
			parser.frame.payload.length = payload_len;
			parser.frame.hlen = 2;
			parser.state = parser.frame.masked ? PARSING_MASK : PARSING_PAYLOAD_PRECHECK;
		} else if (payload_len == 126) {
			parser.frame.hlen = 4;
			parser.state = PARSING_LEN2;
		} else {
			parser.frame.hlen = 10;
			parser.state = PARSING_LEN8;
		}

		if (parser.frame.masked)
			parser.frame.hlen += 4;
	}

	if (parser.state == PARSING_LEN2) {
		if (Websocket_StreamReadableSize(&stream) < 2) return false;

		Websocket_StreamRead(&stream, scratch, 2);
		ptrdiff_t payload_len = (((uint16_t)scratch[0] << 8) | (uint16_t)scratch[1]);
		parser.state = parser.frame.masked ? PARSING_MASK : PARSING_PAYLOAD_PRECHECK;
		parser.frame.payload.length = payload_len;
	}

	if (parser.state == PARSING_LEN8) {
		if (Websocket_StreamReadableSize(&stream) < 8) return false;

		Websocket_StreamRead(&stream, scratch, 8);
		ptrdiff_t payload_len = (
			((uint64_t)scratch[0] << 56) | ((uint64_t)scratch[1] << 48) |
			((uint64_t)scratch[2] << 40) | ((uint64_t)scratch[3] << 32) |
			((uint64_t)scratch[4] << 24) | ((uint64_t)scratch[5] << 16) |
			((uint64_t)scratch[6] << 8) | ((uint64_t)scratch[7] << 0));
		parser.state = parser.frame.masked ? PARSING_MASK : PARSING_PAYLOAD_PRECHECK;
		parser.frame.payload.length = payload_len;
	}

	if (parser.state == PARSING_MASK) {
		if (Websocket_StreamReadableSize(&stream) < 4) return false;

		Websocket_StreamRead(&stream, parser.frame.mask, 4);
		parser.state = PARSING_PAYLOAD_PRECHECK;
	}

	if (parser.state == PARSING_PAYLOAD_PRECHECK) {
		Assert(parser.frame.payload.data == nullptr);

		ptrdiff_t remaining_cap = reader.stream.p2cap - reader.stream.seriallen;
		if (parser.frame.payload.length > remaining_cap) {
			parser.state = PARSING_DROPPED;
			LogWarningEx("Websocket", "Dropped %d bytes. Frame payload too big. Skipped frame", (int)parser.frame.payload.length);
			Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_MESSAGE_TOO_BIG);
		} else {
			parser.state = PARSING_PAYLOAD;
		}
	}

	if (parser.state == PARSING_PAYLOAD) {
		// Append at the end of serial buffer
		ptrdiff_t remaining = parser.frame.payload.length - parser.payload_parsed;
		ptrdiff_t read      = Websocket_StreamRead(&stream, reader.stream.serialized + reader.stream.seriallen, remaining);

		parser.payload_parsed += read;
		reader.stream.seriallen += read;
		remaining -= read;

		if (!remaining) {
			// Correctly point the payload data at the position of just this frame payload
			parser.frame.payload.data = reader.stream.serialized + reader.stream.seriallen - parser.payload_parsed;
			return true;
		}
		return false;
	}

	if (parser.state == PARSING_DROPPED) {
		ptrdiff_t remaining = parser.frame.payload.length - reader.stream.seriallen;
		ptrdiff_t dropped   = Websocket_StreamDrop(&stream, remaining);
		reader.stream.seriallen += dropped;
		remaining -= dropped;
		if (!remaining)
			Websocket_ResetParser(ctx);
	}

	return false;
}

static void Websocket_PushEventAndReadNext(Websocket_Context *ctx, Buffer buffer, int32_t header) {
	Assert(buffer.length <= ctx->readq.buffp2cap);
	memcpy(ctx->reader.curr_node->buff, buffer.data, buffer.length);
	ctx->reader.curr_node->header = header;
	ctx->reader.curr_node->len    = buffer.length;
	Websocket_QueuePush(&ctx->readq, ctx->reader.curr_node);
	ctx->reader.curr_node = nullptr;
	Semaphore_Signal(ctx->readsem);
	ctx->reader.curr_node = Websocket_QueueAlloc(&ctx->readq);
	Websocket_InitReadNode(ctx);
}

static bool Websocket_HandleMessage(Websocket_Context *ctx) {
	Websocket_Frame &frame = ctx->reader.parser.frame;
	Buffer msg             = Buffer(ctx->reader.stream.serialized, ctx->reader.stream.seriallen);

	if (frame.rsv) {
		Websocket_ResetParser(ctx);
		Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
		return false;
	}

	if (ctx->role == WEBSOCKET_ROLE_CLIENT && frame.masked) {
		LogErrorEx("Websocket", "Server sent masked payload. Closing...");
		Websocket_ResetParser(ctx);
		Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
		return false;
	}

	if (ctx->role == WEBSOCKET_ROLE_SERVER && !frame.masked) {
		LogErrorEx("Websocket", "Client sent unmasked payload. Closing...");
		Websocket_ResetParser(ctx);
		Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
		return false;
	}

	if (frame.masked) {
		Websocket_MaskPayload(frame.payload.data, frame.payload.data, frame.payload.length, frame.mask);
	}

	if (frame.opcode & 0x08) {
		// control frame
		if (!frame.fin) {
			LogErrorEx("Websocket", "Server sent fragmented control frame. Closing...");
			Websocket_ResetParser(ctx);
			Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
			return false;
		}

		if (frame.payload.length > 125) {
			LogErrorEx("Websocket", "Server sent control frame with %d bytes payload. Only upto 125 bytes is allowed. Closing...", (int)frame.payload.length);
			Websocket_ResetParser(ctx);
			Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
			return false;
		}

		switch (frame.opcode) {
			case WEBSOCKET_OP_PING: {
				Websocket_PushEventAndReadNext(ctx, msg, frame.header);
				Websocket_ImmediatePong(ctx, msg);
			} break;

			case WEBSOCKET_OP_PONG: {
				Websocket_PushEventAndReadNext(ctx, msg, frame.header);
			} break;

			case WEBSOCKET_OP_CONNECTION_CLOSE: {
				Websocket_PushEventAndReadNext(ctx, msg, frame.header);

				if (ctx->connection == WEBSOCKET_CONNECTED) {
					Websocket_SendImmediateControlMessage(ctx, msg, WEBSOCKET_OP_CONNECTION_CLOSE);
					ctx->connection = WEBSOCKET_RECEIVED_CLOSE;
				} else {
					ctx->connection = WEBSOCKET_DISCONNECTING;
				}
			} break;

			default: {
				Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
				return false;
			}
		}

		Websocket_ResetParser(ctx);
		return true;
	}

	if (frame.fin) {
		if (frame.opcode != WEBSOCKET_OP_CONTINUATION_FRAME) {
			// single frame
			Websocket_PushEventAndReadNext(ctx, msg, frame.header);
		} else {
			// final frame of fragmented frame
			Websocket_PushEventAndReadNext(ctx, msg, ctx->reader.curr_node->header);
		}
	} else {
		int header      = ctx->reader.curr_node->header;
		int prev_fin    = (header & 0x80) >> 7;
		int prev_opcode = (header & 0x0f) >> 0;

		if (prev_opcode) {
			// continuation of fragmented frame
			if (frame.opcode) {
				LogErrorEx("Websocket", "Expected next fragment with 0x0 opcode, but got %d. Closing...", frame.opcode);
				Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
				Websocket_ResetParser(ctx);
				return false;
			}
		} else {
			// first part of fragmented frame
			if (!frame.opcode) {
				LogErrorEx("Websocket", "Expected next fragment with 0x0 opcode, but got %d. Closing...", frame.opcode);
				Websocket_SendImmediateClose(ctx, WEBSOCKET_CLOSE_PROTOCOL_ERROR);
				Websocket_ResetParser(ctx);
				return false;
			}
			ctx->reader.curr_node->header = frame.header;
		}
	}

	Websocket_ResetParser(ctx);
	return true;
}

static int Websocket_ThreadProc(void *arg) {
	Net_Socket *websocket  = (Net_Socket *)arg;
	Websocket_Context *ctx = (Websocket_Context *)Net_GetUserBuffer(websocket);

	pollfd fd;
	fd.fd = Net_GetSocketDescriptor(websocket);

	while (ctx->connection != WEBSOCKET_DISCONNECTING) {
		fd.events  = 0;
		fd.revents = 0;

		if (Websocket_HasWrite(ctx))
			fd.events |= POLLWRNORM;

		if (Websocket_HasRead(ctx))
			fd.events |= POLLRDNORM;

		int presult = poll(&fd, 1, WEBSOCKET_MAX_WAIT_MS);

		if (presult <= 0) continue;

		if (fd.revents & POLLWRNORM) {
			if (ctx->writer.control.length && !ctx->writer.normal.written) {
				// Send control frames all at once since they will be replaced with another control frame
				// if control frames are sent, and breaking them will cause error
				ptrdiff_t remaining = ctx->writer.control.length;
				uint8_t *write_ptr  = ctx->writer.control.buffer;
				while (remaining) {
					int sent = Net_SendBlocked(websocket, write_ptr, (int)remaining);
					if (sent < 0) {
						LogErrorEx("Websocket", "Connection lost abrubtly");
						ctx->connection = WEBSOCKET_DISCONNECTING;
						return 1;
					}
					remaining -= sent;
					write_ptr += sent;
				}

				int opcode = (ctx->writer.control.buffer[0] & 0x0f) >> 0;
				Websocket_InspectWriteFrameForClose(ctx, opcode);
			} else if (ctx->writer.normal.curr_node) {
				ptrdiff_t remaining = ctx->writer.normal.curr_node->len - ctx->writer.normal.written;
				uint8_t *write_ptr  = ctx->writer.normal.curr_node->buff + ctx->writer.normal.written;

				while (remaining) {
					int bytes_sent = Net_Send(websocket, write_ptr, (int)remaining);
					if (bytes_sent < 0) return 1;
					if (bytes_sent == 0) break;
					remaining -= bytes_sent;
					write_ptr += bytes_sent;
					ctx->writer.normal.written += bytes_sent;
				}

				if (!remaining) {
					Websocket_InspectWriteFrameForClose(ctx, ctx->writer.normal.curr_node->header & 0x0f);
					ctx->writer.normal.written = 0;
					Websocket_QueueFree(&ctx->writeq, ctx->writer.normal.curr_node);
					ctx->writer.normal.curr_node = Websocket_QueuePop(&ctx->writeq);
					Semaphore_Signal(ctx->writesem);
				}
			}
		}

		if (fd.revents & POLLRDNORM) {
			const auto reader = [websocket](uint8_t *buff, ptrdiff_t len) -> ptrdiff_t {
				return Net_Receive(websocket, buff, (int)len);
			};
			
			if (!Websocket_StreamDump(&ctx->reader.stream, reader)) {
				LogErrorEx("Websocket", "Connection lost abrubtly");
				ctx->connection = WEBSOCKET_DISCONNECTING;
				return 1;
			}

			while (Websocket_ParseFrame(ctx)) {
				if (Websocket_HandleMessage(ctx) && ctx->reader.curr_node)
					continue;
			}
		}

		if (fd.revents & (POLLHUP | POLLERR)) {
			LogErrorEx("Websocket", "Connection lost abrubtly");
			ctx->connection = WEBSOCKET_DISCONNECTING;
			return 1;
		}
	}

	return 0;
}

//
//
//

bool Websocket_IsConnected(Websocket *websocket) {
	Net_Socket *sock = (Net_Socket *)websocket;
	Websocket_Context *context = (Websocket_Context *)Net_GetUserBuffer(sock);
	return context->connection != WEBSOCKET_DISCONNECTED;
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

int Websocket_EventCloseCode(const Websocket_Event &event) {
	Assert(event.type == WEBSOCKET_EVENT_CLOSE);
	if (event.message.length >= 2) {
		int a    = event.message[0];
		int b    = event.message[1];
		int code = ((a << 8) | b);
		return code;
	}
	return 0;
}

String Websocket_EventCloseMessage(const Websocket_Event &event) {
	Assert(event.type == WEBSOCKET_EVENT_CLOSE);
	String reason = event.message;
	reason = StrRemovePrefix(reason, 2);
	return reason;
}

Websocket_Result Websocket_Send(Websocket *websocket, String raw_data, Websocket_Opcode opcode, int timeout) {
	Net_Socket *socket     = (Net_Socket *)websocket;
	Websocket_Context *ctx = (Websocket_Context *)Net_GetUserBuffer(socket);

	ptrdiff_t packet_size = Websocket_GetFrameSize(websocket, raw_data.length);
	if (packet_size > ctx->writeq.buffp2cap)
		return WEBSOCKET_E_NOMEM;

	int res = Semaphore_Wait(ctx->writesem, timeout);
	if (res == 0) return WEBSOCKET_E_WAIT;
	if (res > 0) {
		if (ctx->connection == WEBSOCKET_DISCONNECTING || 
			ctx->connection == WEBSOCKET_DISCONNECTED || 
			ctx->connection == WEBSOCKET_SENT_CLOSE)
			return WEBSOCKET_E_CLOSED;
		Websocket_Queue::Node *node = Websocket_QueueAlloc(&ctx->writeq);

		bool masked  = ctx->role == WEBSOCKET_ROLE_CLIENT;
		node->len    = Websocket_CreateFrame(node->buff, ctx->writeq.buffp2cap, raw_data, masked, opcode);
		node->header = node->buff[0];
		Websocket_QueuePush(&ctx->writeq, node);
		return WEBSOCKET_OK;
	}

	return WEBSOCKET_E_SYSTEM;
}

Websocket_Result Websocket_SendText(Websocket *websocket, String raw_data, int timeout) {
	return Websocket_Send(websocket, raw_data, WEBSOCKET_OP_TEXT_FRAME, timeout);
}

Websocket_Result Websocket_SendBinary(Websocket *websocket, String raw_data, int timeout) {
	return Websocket_Send(websocket, raw_data, WEBSOCKET_OP_BINARY_FRAME, timeout);
}

Websocket_Result Websocket_Ping(Websocket *websocket, String raw_data, int timeout) {
	Assert(raw_data.length <= 125);
	return Websocket_Send(websocket, raw_data, WEBSOCKET_OP_PING, timeout);
}

Websocket_Result Websocket_Close(Websocket *websocket, Websocket_Close_Reason reason, int timeout) {
	uint8_t payload[125];

	payload[0] = ((0xff00 & reason) >> 8);
	payload[1] = 0xff & reason;

	String message = Websocket_CloseReasonMessage(reason);
	Assert(message.length <= sizeof(payload) - 2);
	memcpy(payload + 2, message.data, message.length);

	return Websocket_Send(websocket, String(payload, message.length + 2), WEBSOCKET_OP_CONNECTION_CLOSE, timeout);
}

Websocket_Result Websocket_Close(Websocket *websocket, int reason, String data, int timeout) {
	uint8_t payload[125];

	payload[0] = ((0xff00 & reason) >> 8);
	payload[1] = 0xff & reason;

	Assert(data.length <= sizeof(payload) - 2);
	memcpy(payload + 2, data.data, data.length);

	return Websocket_Send(websocket, String(payload, data.length + 2), WEBSOCKET_OP_CONNECTION_CLOSE, timeout);
}

static Websocket_Event_Type Websocket_OpcodeToEventType(int opcode) {
	switch (opcode) {
		case WEBSOCKET_OP_TEXT_FRAME:       return WEBSOCKET_EVENT_TEXT;
		case WEBSOCKET_OP_BINARY_FRAME:     return WEBSOCKET_EVENT_BINARY;
		case WEBSOCKET_OP_PING:             return WEBSOCKET_EVENT_PING;
		case WEBSOCKET_OP_PONG:             return WEBSOCKET_EVENT_PONG;
		case WEBSOCKET_OP_CONNECTION_CLOSE: return WEBSOCKET_EVENT_CLOSE;
		NoDefaultCase();
	}
	return WEBSOCKET_EVENT_CLOSE;
}

static Websocket_Queue::Node *Websocket_ReceiveNode(Websocket_Context *ctx, Websocket_Result *res, int timeout) {
	int wait = Semaphore_Wait(ctx->readsem, timeout);
	if (wait == 0) {
		if (ctx->connection == WEBSOCKET_DISCONNECTING) {
			ctx->connection = WEBSOCKET_DISCONNECTED;
			*res = WEBSOCKET_E_CLOSED;
			return nullptr;
		}
		*res = WEBSOCKET_E_WAIT;
		return nullptr;
	}

	if (wait > 0)
		return Websocket_QueuePop(&ctx->readq);
	*res = WEBSOCKET_E_SYSTEM;
	return nullptr;
}

Websocket_Result Websocket_Receive(Websocket *websocket, Websocket_Event *event, uint8_t *buff, ptrdiff_t bufflen, int timeout) {
	Net_Socket *socket     = (Net_Socket *)websocket;
	Websocket_Context *ctx = (Websocket_Context *)Net_GetUserBuffer(socket);

	Websocket_Result res;
	Websocket_Queue::Node *node = Websocket_ReceiveNode(ctx, &res, timeout);
	if (node) {
		event->type = Websocket_OpcodeToEventType(node->header & 0x0f);

		event->message.data = buff;
		if (node->len <= bufflen) {
			memcpy(event->message.data, node->buff, node->len);
			event->message.length = node->len;
			res = WEBSOCKET_OK;
		} else {
			LogWarningEx("Websocket", "Full frame not read. Reason: Out of memory");
			memcpy(event->message.data, node->buff, bufflen);
			event->message.length = bufflen;
			res = WEBSOCKET_E_NOMEM;
		}

		Websocket_QueueFree(&ctx->readq, node);
	}

	return res;
}

Websocket_Result Websocket_Receive(Websocket *websocket, Websocket_Event *event, Memory_Arena *arena, int timeout) {
	Net_Socket *socket = (Net_Socket *)websocket;
	Websocket_Context *ctx = (Websocket_Context *)Net_GetUserBuffer(socket);

	Websocket_Result res;
	Websocket_Queue::Node *node = Websocket_ReceiveNode(ctx, &res, timeout);
	if (node) {
		event->type = Websocket_OpcodeToEventType(node->header & 0x0f);

		uint8_t *buff = (uint8_t *)PushSize(arena, node->len);
		if (buff) {
			event->message.data = buff;
			memcpy(event->message.data, node->buff, node->len);
			event->message.length = node->len;
			Websocket_QueueFree(&ctx->readq, node);
			return WEBSOCKET_OK;
		}

		LogWarningEx("Websocket", "Full frame not read. Reason: Out of memory");

		ptrdiff_t len = MemoryArenaEmptySize(arena);
		buff = (uint8_t *)PushSize(arena, len);
		if (buff) {
			event->message.data = buff;
			memcpy(event->message.data, node->buff, len);
			event->message.length = len;
		} else {
			event->message = Buffer();
		}

		Websocket_QueueFree(&ctx->readq, node);
		return WEBSOCKET_E_NOMEM;
	}

	return res;
}
