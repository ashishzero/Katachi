#include "Kr/KrBasic.h"
#include "Network.h"
#include "Discord.h"
#include "Kr/KrString.h"
#include "StringBuilder.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "Json.h"
#include "SHA1/sha1.h"

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
// https://discord.com
//

#include "Http.h"
#include "Base64.h"

static uint32_t XorShift32Seed = 0xfdfdfdfd;

static inline uint32_t XorShift32() {
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

static void Websocket_MaskPayload(uint8_t *dst, uint8_t *src, uint64_t length, uint8_t mask[4]) {
	for (uint64_t i = 0; i < length; ++i)
		dst[i] = src[i] ^ mask[i & 3];
}

static int Websocket_ReceiveMinimum(Net_Socket *websocket, uint8_t *read_ptr, int buffer_size, int minimum_req) {
	Assert(minimum_req <= buffer_size);
	int bytes_received = 0;
	while (bytes_received < minimum_req) {
		int bytes_read = Net_Receive(websocket, read_ptr, buffer_size);
		if (bytes_read < 0) return bytes_read;
		bytes_received += bytes_read;
		read_ptr += bytes_read;
		buffer_size -= bytes_read;
	}
	return bytes_received;
}

enum Websocket_Opcode {
	WEBSOCKET_OP_CONTINUATION_FRAME = 0x0,
	WEBSOCKET_OP_TEXT_FRAME = 0x1,
	WEBSOCKET_OP_BINARY_FRAME = 0x2,
	WEBSOCKET_OP_CONNECTION_CLOSE = 0x8,
	WEBSOCKET_OP_PING = 0x9,
	WEBSOCKET_OP_PONG = 0xa
};

enum Websocket_Status {
	WEBSOCKET_STATUS_NORMAL_CLOSURE = 1000,
	WEBSOCKET_STATUS_GOING_AWAY = 1001,
	WEBSOCKET_STATUS_PROTOCOL_ERROR = 1002,
	WEBSOCKET_STATUS_UNSUPPORTED_DATA = 1003,
	WEBSOCKET_STATUS_NOT_RECEIVED = 1005,
	WEBSOCKET_STATUS_ABNORMAL_CLOSURE = 1006,
	WEBSOCKET_STATUS_INVALID_FRAME_PAYLOAD_DATA = 1007,
	WEBSOCKET_STATUS_POLICY_VOILATION = 1008,
	WEBSOCKET_STATUS_MESSAGE_TOO_BIG = 1009,
	WEBSOCKET_STATUS_MANDATORY_EXTENSION = 1010,
	WEBSOCKET_STATUS_INTERNAL_SERVER_ERROR = 1011,
	WEBSOCKET_STATUS_TLS_HANDSHAKE = 1012
};

enum Websocket_Result {
	WEBSOCKET_OK,
	WEBSOCKET_FAILED,
	WEBSOCKET_TIMED_OUT,
	WEBSOCKET_OUT_OF_MEMORY,
};

constexpr int WEBSOCKET_STREAM_SIZE = KiloBytes(8);


struct Websocket_Frame {
	int fin;
	int rsv;
	int opcode;
	int masked;
	uint8_t mask[4];
	int header_length;
	uint64_t payload_length;
	uint8_t *payload;
};

Websocket_Result Websocket_Receive(Net_Socket *websocket, Memory_Arena *arena, Websocket_Frame *frame) {
	uint8_t stream[WEBSOCKET_STREAM_SIZE];

	int bytes_received = Websocket_ReceiveMinimum(websocket, stream, WEBSOCKET_STREAM_SIZE, 2);
	if (bytes_received < 0) return WEBSOCKET_FAILED;

	int fin    = (stream[0] & 0x80) >> 7;
	int rsv    = (stream[0] & 0x70) >> 4;
	int opcode = (stream[0] & 0x0f) >> 0;
	int mask   = (stream[1] & 0x80) >> 7;

	uint64_t payload_len = (stream[1] & 0x7f);

	int consumed_size = 2;

	if (payload_len == 126) {
		if (bytes_received < 4) {
			int bytes_read = Websocket_ReceiveMinimum(websocket, stream + bytes_received, WEBSOCKET_STREAM_SIZE - bytes_received, 4 - bytes_received);
			if (bytes_read < 0) return WEBSOCKET_FAILED;
			bytes_received += bytes_read;
		}
		consumed_size = 4;
		payload_len = (((uint16_t)stream[3] << 8) | (uint16_t)stream[2]);
	} else if (payload_len == 127) {
		if (bytes_received < 10) {
			int bytes_read = Websocket_ReceiveMinimum(websocket, stream + bytes_received, WEBSOCKET_STREAM_SIZE - bytes_received, 4 - bytes_received);
			if (bytes_read < 0) return WEBSOCKET_FAILED;
			bytes_received += bytes_read;
		}

		consumed_size = 10;
		payload_len = (((uint64_t)stream[9] << 56)  | ((uint64_t)stream[8] << 48) |
						((uint64_t)stream[7] << 40) | ((uint64_t)stream[6] << 32) |
						((uint64_t)stream[5] << 24) | ((uint64_t)stream[4] << 16) |
						((uint64_t)stream[3] << 8)  | ((uint64_t)stream[2] << 0));
	}

	auto mpoint = BeginTemporaryMemory(arena);

	uint8_t *payload = (uint8_t *)PushSize(arena, payload_len);

	if (payload) {
		int payload_read = bytes_received - consumed_size;
		memcpy(payload, stream + consumed_size, payload_read);

		uint8_t *read_ptr = payload + payload_read;
		uint64_t remaining = payload_len - payload_read;

		while (remaining) {
			int bytes_read = Net_Receive(websocket, read_ptr, (int)remaining);
			if (bytes_read < 0) {
				EndTemporaryMemory(&mpoint);
				return WEBSOCKET_FAILED;
			}
			read_ptr += bytes_read;
			remaining -= bytes_read;
		}

		frame->fin             = fin;
		frame->rsv             = rsv;
		frame->opcode          = opcode;
		frame->masked          = mask;
		frame->payload_length  = payload_len;
		frame->payload         = payload;

		return WEBSOCKET_OK;
	}

	return WEBSOCKET_OUT_OF_MEMORY;
}

Websocket_Result Websocket_SendText(Net_Socket *websocket, uint8_t *buffer, ptrdiff_t length, Memory_Arena *arena) {
	uint8_t header[14];
	memset(header, 0, sizeof(header));

	header[0] |= 0x80; // FIN
	header[0] |= WEBSOCKET_OP_TEXT_FRAME; // opcode
	header[1] |= 0x80; // mask

	int header_size;

	// payload length
	if (length <= 125) {
		header_size = 6;
		header[1] |= (uint8_t)length;
	} else if (length <= UINT16_MAX) {
		header_size = 8;
		header[1] |= 126;
		header[2] = ((length & 0xff00) >> 8);
		header[3] = ((length & 0x00ff) >> 0);
	} else {
		header_size = 14;
		header[1] |= 127;
		header[2] = (uint8_t)((length & 0xff00000000000000) >> 56);
		header[3] = (uint8_t)((length & 0x00ff000000000000) >> 48);
		header[4] = (uint8_t)((length & 0x0000ff0000000000) >> 40);
		header[5] = (uint8_t)((length & 0x000000ff00000000) >> 32);
		header[6] = (uint8_t)((length & 0x00000000ff000000) >> 24);
		header[7] = (uint8_t)((length & 0x0000000000ff0000) >> 16);
		header[8] = (uint8_t)((length & 0x000000000000ff00) >> 8);
		header[9] = (uint8_t)((length & 0x00000000000000ff) >> 0);
	}

	uint32_t mask = XorShift32();
	uint8_t *mask_write = header + header_size - 4;
	mask_write[0] = ((mask & 0xff000000) >> 24);
	mask_write[1] = ((mask & 0x00ff0000) >> 16);
	mask_write[2] = ((mask & 0x0000ff00) >> 8);
	mask_write[3] = ((mask & 0x000000ff) >> 0);

	auto temp = BeginTemporaryMemory(arena);
	Defer{ EndTemporaryMemory(&temp); };

	uint8_t *frame = (uint8_t *)PushSize(arena, header_size + length);
	if (frame) {
		memcpy(frame, header, header_size);
		Websocket_MaskPayload(frame + header_size, buffer, length, mask_write);

		uint8_t *write_ptr = frame;
		uint64_t remaining = header_size + length;
		while (remaining) {
			int bytes_written = Net_Send(websocket, write_ptr, (int)remaining);
			if (bytes_written < 0) return WEBSOCKET_FAILED;
			write_ptr += bytes_written;
			remaining -= bytes_written;
		}

		return WEBSOCKET_OK;
	}

	return WEBSOCKET_OUT_OF_MEMORY;
}

#include "NetworkNative.h"

// @Improvement: This is discord limitation not websocket's limitation
constexpr int WEBSOCKET_MAX_WRITE_SIZE = 1024 * 64;

constexpr int WEBSOCKET_MAX_READ_SIZE = 1024 * 64;

// FIFO
// start = stop    : empty
// start = stop + 1: full
struct Websocket_Frame_Writer {
	uint8_t *buffer;
	ptrdiff_t start;
	ptrdiff_t stop;
};

static void Websocket_FrameWriterInit(Websocket_Frame_Writer *writer, Memory_Arena *arena) {
	writer->buffer = PushArray(arena, uint8_t, WEBSOCKET_MAX_WRITE_SIZE);
	writer->start = writer->stop = 0;
}

static bool Websocket_FrameWriterWrite(Websocket_Frame_Writer *writer, uint8_t *src, ptrdiff_t len) {
	ptrdiff_t cap;

	if (writer->stop >= writer->start) {
		cap = WEBSOCKET_MAX_WRITE_SIZE - writer->stop;
		cap += writer->start - 1;
	} else {
		cap = writer->start - writer->stop - 1;
	}

	if (len > cap) return false;

	if (writer->stop > writer->start) {
		ptrdiff_t write_size = Minimum(len, WEBSOCKET_MAX_WRITE_SIZE - writer->stop);
		memcpy(writer->buffer + writer->stop, src, write_size);
		if (write_size < len) {
			memcpy(writer->buffer, src + write_size, len - write_size);
		}
	} else {
		memcpy(writer->buffer + writer->stop, src, len);
	}

	writer->stop = (writer->stop + len) & (WEBSOCKET_MAX_WRITE_SIZE - 1);

	return true;
}

static String Websocket_CreateTextFrame(Net_Socket *websocket, uint8_t *payload, ptrdiff_t length, int masked, Memory_Arena *arena) {
	uint8_t header[14]; // the last 4 bytes are for mask but is not actually used
	memset(header, 0, sizeof(header));

	header[0] |= 0x80; // FIN
	header[0] |= WEBSOCKET_OP_TEXT_FRAME; // opcode
	header[1] |= masked ? 0x80 : 0x00; // mask

	int header_size;

	// payload length
	if (length <= 125) {
		header_size = 2;
		header[1] |= (uint8_t)length;
	} else if (length <= UINT16_MAX) {
		header_size = 4;
		header[1] |= 126;
		header[2] = ((length & 0xff00) >> 8);
		header[3] = ((length & 0x00ff) >> 0);
	} else {
		header_size = 10;
		header[1] |= 127;
		header[2] = (uint8_t)((length & 0xff00000000000000) >> 56);
		header[3] = (uint8_t)((length & 0x00ff000000000000) >> 48);
		header[4] = (uint8_t)((length & 0x0000ff0000000000) >> 40);
		header[5] = (uint8_t)((length & 0x000000ff00000000) >> 32);
		header[6] = (uint8_t)((length & 0x00000000ff000000) >> 24);
		header[7] = (uint8_t)((length & 0x0000000000ff0000) >> 16);
		header[8] = (uint8_t)((length & 0x000000000000ff00) >> 8);
		header[9] = (uint8_t)((length & 0x00000000000000ff) >> 0);
	}

	if (masked)
		header_size += 4;

	ptrdiff_t frame_size = header_size + length;

	uint8_t *frame = (uint8_t *)PushSize(arena, frame_size);
	if (!frame) {
		LogErrorEx("Websocket", "Failed to create websocket frame: out of memory");
		return String("");
	}

	memcpy(frame, header, header_size);

	if (masked) {
		uint32_t mask = XorShift32();
		uint8_t *mask_write = frame + header_size - 4;
		mask_write[0] = ((mask & 0xff000000) >> 24);
		mask_write[1] = ((mask & 0x00ff0000) >> 16);
		mask_write[2] = ((mask & 0x0000ff00) >> 8);
		mask_write[3] = ((mask & 0x000000ff) >> 0);

		Websocket_MaskPayload(frame + header_size, payload, length, mask_write);
	} else {
		memcpy(frame + header_size, payload, length);
	}

	return String(frame, frame_size);
}

struct Websocket_Frame_Reader {
	enum State { READ_HEADER, READ_LEN2, READ_LEN8, READ_MASK, READ_PAYLOAD };
	State           state;
	ptrdiff_t       start;
	ptrdiff_t       stop;
	uint8_t *       buffer;
	uint64_t        payload_read;
	Websocket_Frame frame;
};

static void Websocket_FrameReaderInit(Websocket_Frame_Reader *reader, Memory_Arena *arena) {
	reader->state        = Websocket_Frame_Reader::READ_HEADER;
	reader->start        = 0;
	reader->stop         = 0;
	reader->buffer       = (uint8_t *)PushSize(arena, WEBSOCKET_MAX_READ_SIZE);
	reader->payload_read = 0;
	memset(&reader->frame, 0, sizeof(reader->frame));
}

static void Websocket_FrameReaderNext(Websocket_Frame_Reader *reader) {
	reader->state        = Websocket_Frame_Reader::READ_HEADER;
	reader->payload_read = 0;
	memset(&reader->frame, 0, sizeof(reader->frame));
}

static bool Websocket_FrameReaderUpdate(Websocket_Frame_Reader *reader, Memory_Arena *arena) {
	// @todo: loop until we are done for real

	ptrdiff_t length;
	if (reader->stop >= reader->start) {
		length = reader->stop - reader->start;
	} else {
		length = reader->stop + WEBSOCKET_MAX_READ_SIZE - reader->start;
	}

	if (reader->state == Websocket_Frame_Reader::READ_HEADER) {
		if (length < 2) return false;

		ptrdiff_t i = reader->start;
		ptrdiff_t j = (i + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
		reader->start = (reader->start + 2) & (WEBSOCKET_MAX_READ_SIZE - 1);

		// @todo: validiate frame header for framentation and store relevant/needed values

		reader->frame.fin    = (reader->buffer[i] & 0x80) >> 7;
		reader->frame.rsv    = (reader->buffer[i] & 0x70) >> 4;
		reader->frame.opcode = (reader->buffer[i] & 0x0f) >> 0;
		reader->frame.masked = (reader->buffer[j] & 0x80) >> 7;
		uint64_t payload_len = (reader->buffer[j] & 0x7f);

		if (payload_len <= 125) {
			reader->frame.payload_length += payload_len;
			reader->frame.header_length = 2;
			reader->state = reader->frame.masked ? Websocket_Frame_Reader::READ_MASK : Websocket_Frame_Reader::READ_PAYLOAD;

			//Assert(payload_len);

			uint8_t *mem = (uint8_t *)PushSize(arena, payload_len);
			Assert(mem);// @todo: mem cleanup
			if (!reader->frame.payload) {
				reader->frame.payload = mem;
			} else {
				Assert(mem == reader->frame.payload == reader->frame.payload_length);
			}
		} else if (payload_len == 126) {
			reader->frame.header_length = 4;
			reader->state = Websocket_Frame_Reader::READ_LEN2;
		} else {
			reader->frame.header_length = 10;
			reader->state = Websocket_Frame_Reader::READ_LEN8;
		}

		if (reader->frame.masked)
			reader->frame.header_length += 4;
	}

	if (reader->state == Websocket_Frame_Reader::READ_LEN2) {
		if (length >= 2) {
			ptrdiff_t i = reader->start;
			ptrdiff_t j = (i + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
			reader->start = (reader->start + 2) & (WEBSOCKET_MAX_READ_SIZE - 1);

			uint64_t payload_len = (((uint16_t)reader->buffer[i] << 8) | (uint16_t)reader->buffer[j]);
			reader->state = reader->frame.masked ? Websocket_Frame_Reader::READ_MASK : Websocket_Frame_Reader::READ_PAYLOAD;

			//Assert(payload_len);

			reader->frame.payload_length += payload_len;

			uint8_t *mem = (uint8_t *)PushSize(arena, payload_len);
			Assert(mem);// @todo: mem cleanup
			if (!reader->frame.payload) {
				reader->frame.payload = mem;
			} else {
				Assert(mem == reader->frame.payload == payload_len);
			}
		}
	}

	if (reader->state == Websocket_Frame_Reader::READ_LEN8) {
		if (length >= 8) {
			ptrdiff_t i8 = reader->start;
			ptrdiff_t i7 = (i8 + 0) & (WEBSOCKET_MAX_READ_SIZE - 1);
			ptrdiff_t i6 = (i7 + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
			ptrdiff_t i5 = (i6 + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
			ptrdiff_t i4 = (i5 + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
			ptrdiff_t i3 = (i4 + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
			ptrdiff_t i2 = (i3 + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
			ptrdiff_t i1 = (i2 + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
			ptrdiff_t i0 = (i1 + 1) & (WEBSOCKET_MAX_READ_SIZE - 1);
			reader->start = (reader->start + 8) & (WEBSOCKET_MAX_READ_SIZE - 1);

			uint64_t payload_len = (
					((uint64_t)reader->buffer[i7] << 56) | ((uint64_t)reader->buffer[i6] << 48) |
					((uint64_t)reader->buffer[i5] << 40) | ((uint64_t)reader->buffer[i4] << 32) |
					((uint64_t)reader->buffer[i3] << 24) | ((uint64_t)reader->buffer[i2] << 16) |
					((uint64_t)reader->buffer[i1] <<  8) | ((uint64_t)reader->buffer[i0] << 0));
			reader->state = reader->frame.masked ? Websocket_Frame_Reader::READ_MASK : Websocket_Frame_Reader::READ_PAYLOAD;

			//Assert(payload_len);

			reader->frame.payload_length += payload_len;

			uint8_t *mem = (uint8_t *)PushSize(arena, payload_len);
			Assert(mem);// @todo: mem cleanup
			if (!reader->frame.payload) {
				reader->frame.payload = mem;
			} else {
				Assert(mem == reader->frame.payload == payload_len);
			}
		}
	}

	if (reader->state == Websocket_Frame_Reader::READ_MASK) {
		if (length >= 4) {
			memcpy(reader->frame.mask, reader->buffer + reader->start, 4);
			reader->start = (reader->start + 4) & (WEBSOCKET_MAX_READ_SIZE - 1);
			reader->state = Websocket_Frame_Reader::READ_PAYLOAD;
		}
	}

	if (reader->state == Websocket_Frame_Reader::READ_PAYLOAD) {
		Assert(reader->frame.payload);

		if (reader->stop < reader->start) {
			ptrdiff_t copy_size = Minimum(WEBSOCKET_MAX_READ_SIZE - reader->start, (ptrdiff_t)(reader->frame.payload_length - reader->payload_read));
			memcpy(reader->frame.payload + reader->payload_read, reader->buffer + reader->start, copy_size);
			reader->payload_read += copy_size;
			reader->start = (reader->start + copy_size) & (WEBSOCKET_MAX_READ_SIZE - 1);
		}

		if (reader->stop > reader->start) {
			ptrdiff_t copy_size = Minimum(reader->stop - reader->start, (ptrdiff_t)(reader->frame.payload_length - reader->payload_read));
			memcpy(reader->frame.payload + reader->payload_read, reader->buffer + reader->start, copy_size);
			reader->payload_read += copy_size;
			reader->start = (reader->start + copy_size) & (WEBSOCKET_MAX_READ_SIZE - 1);
		}

		return reader->frame.fin && reader->payload_read == reader->frame.payload_length;
	}

	return false;
}

//
// @Notes:
// 
//

namespace Discord {
	enum Gateway_Opcode {
		GATEWAY_OP_DISPATH = 0,
		GATEWAY_OP_HEARTBEAT = 1,
		GATEWAY_OP_IDENTIFY = 2,
		GATEWAY_OP_PRESECE_UPDATE = 3,
		GATEWAY_OP_VOICE_STATE_UPDATE = 4,
		GATEWAY_OP_RESUME = 6,
		GATEWAY_OP_RECONNECT = 7,
		GATEWAY_OP_REQUEST_GUILD_MEMBERS = 8,
		GATEWAY_OP_INVALID_SESSION = 9,
		GATEWAY_OP_HELLO = 10,
		GATEWAY_OP_HEARTBEAT_ACK = 11,
	};
}

// these are globals for now
int wait_timeout = NET_TIMEOUT_MILLISECS;

namespace global {
	int timeout = NET_TIMEOUT_MILLISECS;
	bool hello_received = false;
	bool imm_heartbeat = false;
	bool identify = false;
	int heartbeat = 0;
	int acknowledgement = 0;
	int s = -1;
	time_t counter;
}

static String BOT_AUTHORIZATION;

void OnWebsocketMessage(String payload, Memory_Arena *arena) {
	auto temp = BeginTemporaryMemory(arena);
	Defer{ EndTemporaryMemory(&temp); };

	Json json_event;
	if (!JsonParse(payload, &json_event, MemoryArenaAllocator(arena))) {
		LogError("Invalid JSON received: %.*s", (int)payload.length, payload.data);
		return;
	}
	Assert(json_event.type == JSON_TYPE_OBJECT);
	Json_Object *obj = &json_event.value.object;

	int opcode = (int)JsonObjectFind(obj, "op")->value.number.value.integer;

	auto s = JsonObjectFind(obj, "s");
	if (s && s->type == JSON_TYPE_NUMBER) {
		global::s = s->value.number.value.integer;
	}

	auto t = JsonObjectFind(obj, "t");
	if (t && t->type == JSON_TYPE_STRING) {
		String name = t->value.string;
		LogInfoEx("Discord Event", "%.*s", (int)name.length, name.data);

#if 0
		if (name == "MESSAGE_CREATE") {
			Net_Socket *http = Http_Connect("https://discord.com");
			if (http) {
				auto temp = BeginTemporaryMemory(arena);

				static const String ReadYourMsg = u8R"foo(
{
  "content": "I read your message"
}
)foo";

				Http_Request req;
				Http_InitRequest(&req);
				Http_SetHost(&req, http);
				Http_SetHeader(&req, HTTP_HEADER_AUTHORIZATION, BOT_AUTHORIZATION);
				Http_SetHeader(&req, HTTP_HEADER_USER_AGENT, "Katachi");
				Http_SetHeader(&req, HTTP_HEADER_CONNECTION, "keep-alive");
				Http_SetContent(&req, "application/json", ReadYourMsg);

				Http_Response res;
				if (Http_Post(http, "/api/v9/channels/850062914438430751/messages", req, &res, arena)) {
					printf(StrFmt "\n", StrArg(res.body));
				}

				Http_Disconnect(http);

				EndTemporaryMemory(&temp);
			}
		}
#endif
	}

	switch (opcode) {
		case Discord::GATEWAY_OP_HELLO: {
			auto d = &JsonObjectFind(obj, "d")->value.object;
			global::timeout = JsonObjectFind(d, "heartbeat_interval")->value.number.value.integer;
			global::counter = clock();
			global::hello_received = true;
			global::identify = true;
			LogInfoEx("Discord Event", "Hello from senpai: %d", global::timeout);
		} break;

		case Discord::GATEWAY_OP_HEARTBEAT: {
			global::imm_heartbeat = true;
			LogInfoEx("Discord Event", "Call from senpai");
		} break;

		case Discord::GATEWAY_OP_HEARTBEAT_ACK: {
			global::acknowledgement += 1;
			LogInfoEx("Discord Event", "Senpai noticed me!");
		} break;

		case Discord::GATEWAY_OP_DISPATH: {
			LogInfoEx("Discord Event", "dispatch");
		} break;

		case Discord::GATEWAY_OP_IDENTIFY: {
			LogInfoEx("Discord Event", "identify");
		} break;

		case Discord::GATEWAY_OP_PRESECE_UPDATE: {
			LogInfoEx("Discord Event", "presence update");
		} break;

		case Discord::GATEWAY_OP_VOICE_STATE_UPDATE: {
			LogInfoEx("Discord Event", "voice state update");
		} break;

		case Discord::GATEWAY_OP_RESUME: {
			LogInfoEx("Discord Event", "resume");
		} break;

		case Discord::GATEWAY_OP_RECONNECT: {
			LogInfoEx("Discord Event", "reconnect");
		} break;

		case Discord::GATEWAY_OP_REQUEST_GUILD_MEMBERS: {
			LogInfoEx("Discord Event", "guild members");
		} break;

		case Discord::GATEWAY_OP_INVALID_SESSION: {
			LogInfoEx("Discord Event", "invalid session");
		} break;
	}
}

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
		parsed->port   = "443"; // default
		parsed->secure = true;
		uri = SubStr(uri, 6);
	} else if (StrStartsWithICase(uri, "ws://")) {
		parsed->scheme = "ws";
		parsed->port   = "80"; // default
		parsed->secure = false;
		uri = SubStr(uri, 5);
	} else {
		// defaults
		parsed->scheme = "wss";
		parsed->port   = "443";
		parsed->secure = true;
		scheme         = false;
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

static constexpr int WEBSOCKET_MAX_PROTOCOLS = 16;

struct Websocket_Procotols {
	ptrdiff_t count;
	String    data[WEBSOCKET_MAX_PROTOCOLS];
};

struct Websocket_Header {
	Http_Header         headers;
	Websocket_Procotols protocols;
};

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

Net_Socket *Websocket_Connect(String uri, Http_Response *res, Websocket_Header *header = nullptr, Memory_Allocator allocator = ThreadContext.allocator) {
	Websocket_Uri websocket_uri;
	if (!Websocket_ParseURI(uri, &websocket_uri)) {
		LogErrorEx("Websocket", "Invalid websocket address: " StrFmt, StrArg(uri));
		return nullptr;
	}

	Net_Socket *websocket = Http_Connect(websocket_uri.host, websocket_uri.port, websocket_uri.secure ? HTTPS_CONNECTION : HTTP_CONNECTION, allocator);
	if (!websocket) return nullptr;

	Http_Request req;
	Http_InitRequest(&req);

	if (header) {
		memcpy(&req.headers, &header->headers, sizeof(req.headers));
	}

	Websocket_Key ws_key = Websocket_GenerateSecurityKey();

	Http_SetHost(&req, websocket);
	Http_SetHeader(&req, HTTP_HEADER_UPGRADE, "websocket");
	Http_SetHeader(&req, HTTP_HEADER_CONNECTION, "Upgrade");
	Http_SetHeader(&req, "Sec-WebSocket-Version", "13");
	Http_SetHeader(&req, "Sec-WebSocket-Key", String(ws_key.data, sizeof(ws_key.data)));
	Http_SetContent(&req, "", "");

	if (header) {
		int length     = 0;
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

	if (Http_Get(websocket, websocket_uri.path, req, res, nullptr, 0) && res->status.code == 101) {
		if (!StrMatchICase(Http_GetHeader(res, HTTP_HEADER_UPGRADE), "websocket")) {
			LogErrorEx("websocket", "Upgrade header is not present in websocket handshake");
			Http_Disconnect(websocket);
			return nullptr;
		}

		if (!StrMatchICase(Http_GetHeader(res, HTTP_HEADER_CONNECTION), "upgrade")) {
			LogErrorEx("websocket", "Connection header is not present in websocket handshake");
			Http_Disconnect(websocket);
			return nullptr;
		}

		String accept = Http_GetHeader(res, "Sec-WebSocket-Accept");
		if (!accept.length) {
			LogErrorEx("websocket", "Sec-WebSocket-Accept header is not present in websocket handshake");
			Http_Disconnect(websocket);
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
			Http_Disconnect(websocket);
			return nullptr;
		}

		String extensions = Http_GetHeader(res, "Sec-WebSocket-Extensions");
		if (extensions.length) {
			LogErrorEx("Websocket", "Extensions are not supported");
			Http_Disconnect(websocket);
			return nullptr;
		}

		String protocols  = Http_GetHeader(res, "Sec-WebSocket-Protocol");

		if (header) {
			Str_Tokenizer tokenizer;
			StrTokenizerInit(&tokenizer, extensions);
			while (StrTokenize(&tokenizer, ",")) {
				for (ptrdiff_t index = 0; index < header->protocols.count; ++index) {
					String prot = StrTrim(tokenizer.token);
					if (!StrMatchICase(prot, header->protocols.data[index])) {
						LogErrorEx("Websocket", "Unsupported Protocol \"" StrFmt "\" sent", StrArg(prot));
						Http_Disconnect(websocket);
						return nullptr;
					}
				}
			}
		} else {
			if (protocols.length) {
				LogErrorEx("Websocket", "Unsupproted protocols sent by server");
				Http_Disconnect(websocket);
				return nullptr;
			}
		}

		return websocket;
	}

	Http_Disconnect(websocket);
	return nullptr;
}

static int NextPowerOf2(int n) {
	int count = 0;
	if (n && !(n & (n - 1)))
		return n;
	while (n != 0) {
		n >>= 1;
		count += 1;
	}
	return 1 << count;
}

int main(int argc, char **argv) {
	InitThreadContext(KiloBytes(64));
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	Memory_Arena *arenas[2];
	arenas[0] = MemoryArenaAllocate(MegaBytes(64));
	arenas[1] = MemoryArenaAllocate(MegaBytes(64));

	Net_Socket *http = Http_Connect("https://discord.com");
	if (!http) {
		return 1;
	}

	BOT_AUTHORIZATION = FmtStr(ThreadScratchpad(), "Bot %s", argv[1]);

	Http_Request req;
	Http_InitRequest(&req);
	Http_SetHost(&req, http);
	Http_SetHeader(&req, HTTP_HEADER_AUTHORIZATION, BOT_AUTHORIZATION);
	Http_SetHeader(&req, HTTP_HEADER_USER_AGENT, "Katachi");
	Http_SetHeader(&req, HTTP_HEADER_CONNECTION, "keep-alive");
	Http_SetContent(&req, "application/json", Message);

	Http_Response res;
	if (Http_Post(http, "/api/v9/channels/850062914438430751/messages", req, &res, arenas[0])) {
		printf(StrFmt "\n", StrArg(res.body));
	}

	Http_Disconnect(http);

	MemoryArenaReset(arenas[0]);

	Websocket_Header headers;
	Websocket_InitHeader(&headers);
	Websocket_HeaderSet(&headers, HTTP_HEADER_AUTHORIZATION, BOT_AUTHORIZATION);
	Websocket_HeaderSet(&headers, HTTP_HEADER_USER_AGENT, "Katachi");

	Net_Socket *websocket = Websocket_Connect("wss://gateway.discord.gg/?v=9&encoding=json", &res, &headers);

	if (!websocket) {
		return 1;
	}

	// A server MUST NOT mask any frames that it sends to
	// the client.A client MUST close a connection if it detects a masked
	// frame.In this case, it MAY use the status code 1002 (protocol
	// error) as defined in Section 7.4.1.

	// RSV1, RSV2, RSV3:  1 bit each
	// MUST be 0 unless an extension is negotiated that defines meanings
	// for non - zero values.If a nonzero value is received and none of
	// the negotiated extensions defines the meaning of such a nonzero
	// value, the receiving endpoint MUST _Fail the WebSocket Connection_.

	// Opcodes
	// %x0 denotes a continuation 
	// %x1 denotes a text frame
	// %x2 denotes a binary frame
	// %x3-7 are reserved for further non-control frames
	// %x8 denotes a connection close
	// %x9 denotes a ping
	// %xA denotes a pong
	// %xB-F are reserved for further control frames

	// The length of the "Payload data", in bytes : if 0 - 125, that is the
	// payload length.If 126, the following 2 bytes interpreted as a
	// 16-bit unsigned integer are the payload length.If 127, the
	// following 8 bytes interpreted as a 64 - bit unsigned integer(the
	// most significant bit MUST be 0) are the payload length.

	// A fragmented message consists of a single frame with the FIN bit
	// clear andan opcode other than 0, followed by zero or more frames
	// with the FIN bit clear and the opcode set to 0, andterminated by
	// a single frame with the FIN bit set andan opcode of 0.

	// Control frames(see Section 5.5) MAY be injected in the middle of
	// a fragmented message.Control frames themselves MUST NOT be
	// fragmented.

	// All control frames MUST have a payload length of 125 bytes or less
	// and MUST NOT be fragmented.

	// Close
	// If there is a body, the first two bytes of
	// the body MUST be a 2 - byte unsigned integer(in network byte order)
	// representing a status code with value / code / defined in Section 7.4.
	// Following the 2 - byte integer, the body MAY contain UTF - 8 - encoded data
	// with value / reason / , the interpretation of which is not defined by
	// this specification.

	// If this Close control frame contains no status code, _The WebSocket
	// Connection Close Code_ is considered to be 1005.  If _The WebSocket
	// Connection is Closed_ and no Close control frame was received by the
	// endpoint(such as could occur if the underlying transport connection
	// is lost), _The WebSocket Connection Close Code_ is considered to be 1006.

	// The first reconnect attempt SHOULD be delayed by a random amount of
	// time.The parameters by which this random delay is chosen are left
	// to the client to decide; a value chosen randomly between 0 and 5
	// seconds is a reasonable initial delay though clients MAY choose a
	// different interval from which to select a delay length based on
	// implementation experience and particular application.
	// Should the first reconnect attempt fail, subsequent reconnect
	// attempts SHOULD be delayed by increasingly longer amounts of time,
	// using a method such as truncated binary exponential backoff.

	// When an endpoint is to interpret a byte stream as UTF - 8 but finds
	// that the byte stream is not, in fact, a valid UTF - 8 stream, that
	// endpoint MUST _Fail the WebSocket Connection_.This rule applies
	// both during the opening handshake andduring subsequent data
	// exchange.

	MemoryArenaReset(arenas[0]);

	auto scratch    = arenas[0];
	auto read_arena = arenas[1]; // resets after every read @todo: better way of doing this

	Websocket_Frame_Writer frame_writer;
	Websocket_FrameWriterInit(&frame_writer, scratch);

	Websocket_Frame_Reader frame_reader;
	Websocket_FrameReaderInit(&frame_reader, read_arena);

	

	bool connected = true;
	while (connected) {
		pollfd fd;
		fd.fd = Net_GetSocketDescriptor(websocket);
		fd.revents = 0;

		fd.events = POLLRDNORM;
		if (frame_writer.start != frame_writer.stop)
			fd.events |= POLLWRNORM;

		int presult = poll(&fd, 1, wait_timeout);
		if (presult < 0) break;

		if (presult > 0) {
			// TODO: Loop until we can no longer sent data
			if (fd.revents & POLLWRNORM) {
				Assert(frame_writer.start != frame_writer.stop);

				int bytes_sent = 0;
				if (frame_writer.stop >= frame_writer.start) {
					bytes_sent = Net_Send(websocket, frame_writer.buffer + frame_writer.start, (int)(frame_writer.stop - frame_writer.start));
				} else {
					bytes_sent = Net_Send(websocket, frame_writer.buffer + frame_writer.start, (int)(WEBSOCKET_MAX_WRITE_SIZE - frame_writer.start));
				}

				if (bytes_sent < 0) break;
				frame_writer.start = (frame_writer.start + bytes_sent) & (WEBSOCKET_MAX_WRITE_SIZE - 1);
			}

			// TODO: Loop until we can't reveive data
			if (fd.revents & POLLRDNORM) {
				int bytes_received;
				if (frame_reader.stop >= frame_reader.start) {
					bytes_received = Net_Receive(websocket, frame_reader.buffer + frame_reader.stop, (int)(WEBSOCKET_MAX_READ_SIZE - frame_reader.stop));
				} else {
					bytes_received = Net_Receive(websocket, frame_reader.buffer + frame_reader.stop, (int)(frame_reader.start - frame_reader.stop - 1));
				}
				if (bytes_received < 0) break;
				frame_reader.stop = (frame_reader.stop + bytes_received) & (WEBSOCKET_MAX_READ_SIZE - 1);

				if (Websocket_FrameReaderUpdate(&frame_reader, read_arena)) {
					if (frame_reader.frame.masked) break; // servers don't mask
					if (frame_reader.frame.opcode & 0x80) {
						if (frame_reader.frame.payload_length > 125) break; // not allowed by specs
						// TODO: handle control frames
						LogInfoEx("Websocket", "control frame");
					} else {
						String payload(frame_reader.frame.payload, frame_reader.frame.payload_length);
						OnWebsocketMessage(payload, read_arena);
					}

					MemoryArenaReset(read_arena);
					Websocket_FrameReaderNext(&frame_reader);
				}
			}
		}

		static uint8_t buffer[4096]; // max allowed discord payload
		if (global::identify) {

			int length = snprintf((char *)buffer, sizeof(buffer), R"foo(
{
  "op": 2,
  "d": {
    "token": "%s",
    "intents": 513,
    "properties": {
      "$os": "Windows"
    }
  }
}
)foo", argv[1]);

			auto temp = BeginTemporaryMemory(scratch);
			//auto send_res = Websocket_SendText(websocket, buffer, length, reader.arena);
			String frame = Websocket_CreateTextFrame(websocket, buffer, length, 1, scratch);
			Websocket_FrameWriterWrite(&frame_writer, frame.data, frame.length);
			EndTemporaryMemory(&temp);

			global::identify = false;

			LogInfoEx("Identify", "Notice me senpai");
		}

		time_t current_counter = clock();
		int time_spent_ms = (int)((current_counter - global::counter) * 1000.0f / (float)CLOCKS_PER_SEC);
		global::counter = current_counter;

		wait_timeout -= time_spent_ms;
		if (wait_timeout <= 0) {
			wait_timeout = global::timeout;
			presult = 0;
		}

		if (presult == 0 || global::imm_heartbeat) {
			if (global::hello_received) {
				if (global::heartbeat != global::acknowledgement) {
					// @todo: only wait for certain number of times
					LogInfoEx("Heartbeat", "Senpai failed to notice me");
				}

				int length = 0;
				if (global::s > 0)
					length = snprintf((char *)buffer, sizeof(buffer), "{\"op\": 1, \"d\":%d}", global::s);
				else
					length = snprintf((char *)buffer, sizeof(buffer), "{\"op\": 1, \"d\":null}");
				auto temp = BeginTemporaryMemory(scratch);
				String frame = Websocket_CreateTextFrame(websocket, buffer, length, 1, scratch);
				Websocket_FrameWriterWrite(&frame_writer, frame.data, frame.length);
				EndTemporaryMemory(&temp);
				global::heartbeat += 1;
				LogInfoEx("Heartbeat", "Notice me senpai");

				global::imm_heartbeat = false;
			}
		}
	}

	Http_Disconnect(websocket);

	Net_Shutdown();

	return 0;
}
