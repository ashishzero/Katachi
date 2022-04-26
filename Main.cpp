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

void Dump(const Http_Header &header, uint8_t *buffer, int length, void *content) {
	printf("%.*s", length, buffer);
}

static const String Base64Lookup = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr size_t Base64EncodedSize(size_t size) {
	ptrdiff_t base64_len = (4 * size) / 3;
	return AlignPower2Up(base64_len, 4);
}

constexpr size_t Based64DecodedSize(size_t size) {
	return (3 * size) / 4;
}

String EncodeBase64(Buffer input, uint8_t *base64, ptrdiff_t length) {
	ptrdiff_t base64_len = Base64EncodedSize(input.length);
	Assert(base64_len <= length);

	int val = 0;
	int valb = -6;

	ptrdiff_t pos = 0;
	String result(base64, base64_len);

	for (ptrdiff_t index = 0; index < input.length; ++index) {
		uint8_t c = input[index];
		val = (val << 8) + static_cast<uint8_t>(c);
		valb += 8;
		while (valb >= 0) {
			result[pos++] = Base64Lookup[(val >> valb) & 0x3F];
			valb -= 6;
		}
	}

	if (valb > -6) {
		result[pos++] = Base64Lookup[((val << 8) >> (valb + 8)) & 0x3F];
	}

	for (; pos < base64_len; ++pos)
		result[pos] = '=';

	return result;
}

Buffer DecodeBase64(String base64, uint8_t *buffer, ptrdiff_t length) {
	while (base64[base64.length - 1] == '=')
		base64.length -= 1;

	Buffer result;
	result.length = Based64DecodedSize(base64.length);
	result.data   = buffer;

	Assert(result.length <= length);

	uint8_t positions[4];
	ptrdiff_t pos = 0;
	ptrdiff_t index = 0;

	for (; index < base64.length - 4; index += 4) {
		positions[0] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index + 0]);
		positions[1] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index + 1]);
		positions[2] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index + 2]);
		positions[3] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index + 3]);

		result[pos++] = (positions[0] << 2) + ((positions[1] & 0x30) >> 4);
		result[pos++] = ((positions[1] & 0xf) << 4) + ((positions[2] & 0x3c) >> 2);
		result[pos++] = ((positions[2] & 0x3) << 6) + positions[3];
	}

	ptrdiff_t it = 0;
	for (; index < base64.length; ++index, ++it)
		positions[it] = (uint8_t)StrFindCharacter(Base64Lookup, base64[index]);
	for (; it < 4; ++it)
		positions[it] = 0;

	uint8_t values[3];
	values[0] = (positions[0] << 2) + ((positions[1] & 0x30) >> 4);
	values[1] = ((positions[1] & 0xf) << 4) + ((positions[2] & 0x3c) >> 2);
	values[2] = ((positions[2] & 0x3) << 6) + positions[3];

	for (it = 0; pos < result.length; ++pos, ++it)
		result[pos] = values[it];

	return result;
}

static constexpr int SecureWebSocketKeySize = 24;

static String GenerateSecureWebSocketKey(uint8_t(&buffer)[SecureWebSocketKeySize]) {
	uint8_t nonce[16];
	for (auto &n : nonce)
		n = 'a'; //(uint32_t)rand();
	static_assert(sizeof(nonce) == 16, "");
	return EncodeBase64(Buffer((uint8_t *)nonce, sizeof(nonce)), buffer, sizeof(buffer));
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

static inline uint32_t XorShift32(uint32_t *state) {
	uint32_t x = *state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*state = x;
	return x;
}

static uint32_t XorShift32State = 0xfdfdfdfd;

enum Websocket_Opcode {
	WEBSOCKET_OP_CONTINUATION_FRAME = 0x0,
	WEBSOCKET_OP_TEXT_FRAME = 0x1,
	WEBSOCKET_OP_BINARY_FRAME = 0x2,
	WEBSOCKET_OP_CONNECTION_CLOSE = 0x8,
	WEBSOCKET_OP_PING = 0x9,
	WEBSOCKET_OP_PONG = 0xa
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

	uint32_t mask = XorShift32(&XorShift32State);
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

// FIFO
struct Websocket_Frame_Writer {
	struct Bucket {
		Bucket *prev;
		Bucket *next;
		ptrdiff_t sent;
		ptrdiff_t length;
		uint8_t buffer[WEBSOCKET_STREAM_SIZE];
	};

	Bucket *first;
	Bucket *last;
	Bucket *free_list;
	ptrdiff_t free_count;

	Memory_Arena *arena;
};

static void Websocket_FrameWriterInit(Websocket_Frame_Writer *writer, Memory_Arena *arena) {
	writer->first      = nullptr;
	writer->last       = nullptr;
	writer->free_list  = nullptr;
	writer->free_count = 0;
	writer->arena      = arena;
}

static bool Websocket_FrameWriterWrite(Websocket_Frame_Writer *writer, uint8_t *src, ptrdiff_t len) {
	int num_buks_req = (int)(AlignPower2Up(len, WEBSOCKET_STREAM_SIZE) / WEBSOCKET_STREAM_SIZE);

	if (num_buks_req == 1 && WEBSOCKET_STREAM_SIZE - writer->last->length <= len) {
		memcpy(writer->last->buffer + writer->last->length, src, len);
		writer->last->length += len;
		return true;
	}

	if (writer->free_count < num_buks_req) {
		auto buks_allocated = num_buks_req - writer->free_count;
		auto buks = PushArrayZero(writer->arena, Websocket_Frame_Writer::Bucket, buks_allocated);
		if (!buks) {
			WriteLogErrorEx("Websocket", "Frame could not be written: out of memory");
			return false;
		}

		for (int index = 0; index < buks_allocated - 1; ++index) {
			buks[index].next = &buks[index + 1];
		}

		buks[buks_allocated - 1].next = writer->free_list;
		writer->free_list = buks;
		writer->free_count += buks_allocated;
	}

	ptrdiff_t remaining = len;
	uint8_t *src_ptr    = src;

	auto write_buk = writer->last;
	if (write_buk->length != WEBSOCKET_STREAM_SIZE) {
		int write_size = WEBSOCKET_STREAM_SIZE - write_buk->length;
		memcpy(write_buk->buffer + write_buk->length, src_ptr, write_size);
		write_buk->length += write_size;
		src_ptr += write_size;
		remaining -= write_size;
	}

	while (remaining >= WEBSOCKET_STREAM_SIZE) {
		auto next_buk = writer->free_list;
		writer->free_list = next_buk->next;
		writer->free_count -= 1;
		next_buk->prev = write_buk;
		write_buk->next = next_buk;
		write_buk = next_buk;
		memcpy(write_buk->buffer, src_ptr, WEBSOCKET_STREAM_SIZE);
		write_buk->sent = 0;
		write_buk->length = WEBSOCKET_STREAM_SIZE;
		src_ptr += WEBSOCKET_STREAM_SIZE;
		remaining -= WEBSOCKET_STREAM_SIZE;
	}

	if (remaining) {
		auto next_buk = writer->free_list;
		writer->free_list = next_buk->next;
		writer->free_count -= 1;
		next_buk->prev = write_buk;
		write_buk->next = next_buk;
		write_buk = next_buk;
		memcpy(write_buk->buffer, src_ptr, remaining);
		write_buk->sent = 0;
		write_buk->length = remaining;
	}

	write_buk->next = nullptr;

	return true;
}

static String Websocket_CreateTextFrame(Net_Socket *websocket, uint8_t *payload, ptrdiff_t length, int masked, Memory_Arena *arena) {
	uint8_t header[14];
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
		WriteLogErrorEx("Websocket", "Failed to create websocket frame: out of memory");
		return String("");
	}

	memcpy(frame, header, header_size);

	if (masked) {
		uint32_t mask = XorShift32(&XorShift32State);
		uint8_t *mask_write = header + header_size - 4;
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
	enum State { READ_HEADER, READ_LEN2, READ_LEN8, READ_PAYLOAD };
	State           state;
	uint8_t *       buffer;
	uint64_t        length;
	uint64_t        allocated;
	Websocket_Frame frame;
	Memory_Arena *  arena;
};

static void Websocket_FrameReaderInit(Websocket_Frame_Reader *reader, Memory_Arena *arena) {
	reader->state     = Websocket_Frame_Reader::READ_HEADER;
	reader->buffer    = (uint8_t *)PushSize(arena, WEBSOCKET_STREAM_SIZE);
	reader->length    = 0;
	reader->allocated = WEBSOCKET_STREAM_SIZE;
	reader->arena     = arena;
	memset(&reader->frame, 0, sizeof(reader->frame));
}

static bool Websocket_FrameReaderUpdate(Websocket_Frame_Reader *reader) {
	if (reader->state == Websocket_Frame_Reader::READ_HEADER) {
		if (reader->length < 2) return false;

		reader->frame.fin    = (reader->buffer[0] & 0x80) >> 7;
		reader->frame.rsv    = (reader->buffer[0] & 0x70) >> 4;
		reader->frame.opcode = (reader->buffer[0] & 0x0f) >> 0;
		reader->frame.masked = (reader->buffer[1] & 0x80) >> 7;
		uint64_t payload_len = (reader->buffer[1] & 0x7f);

		reader->frame.payload_length = payload_len;

		if (payload_len <= 125) {
			reader->frame.header_length = 2;
			reader->state = Websocket_Frame_Reader::READ_PAYLOAD;
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
		if (reader->length >= 4) {
			reader->frame.payload_length = (((uint16_t)reader->buffer[3] << 8) | (uint16_t)reader->buffer[2]);
			reader->state = Websocket_Frame_Reader::READ_PAYLOAD;
		}
	}

	if (reader->state == Websocket_Frame_Reader::READ_LEN8) {
		if (reader->length >= 10) {
			reader->frame.payload_length =
				(((uint64_t)reader->buffer[9] << 56) | ((uint64_t)reader->buffer[8] << 48) |
					((uint64_t)reader->buffer[7] << 40) | ((uint64_t)reader->buffer[6] << 32) |
					((uint64_t)reader->buffer[5] << 24) | ((uint64_t)reader->buffer[4] << 16) |
					((uint64_t)reader->buffer[3] << 8) | ((uint64_t)reader->buffer[2] << 0));
			reader->state = Websocket_Frame_Reader::READ_PAYLOAD;
		}
	}

	if (reader->state == Websocket_Frame_Reader::READ_PAYLOAD) {
		Assert(reader->frame.fin); // TODO: Support for fragmentation

		uint64_t frame_size = reader->frame.header_length + reader->frame.payload_length;
		if (reader->allocated < frame_size) {
			void *mem = PushSize(reader->arena, frame_size - reader->allocated);
			if (!mem) {
				WriteLogErrorEx("Websocket", "Failed to read frame: Out of memory");
			}
			Assert(mem == reader->buffer + reader->allocated);
			reader->allocated = frame_size;
		}

		if (reader->length == frame_size) {
			reader->frame.payload = reader->buffer + reader->frame.header_length;

			if (reader->frame.masked)
				memcpy(reader->frame.mask, reader->buffer + reader->frame.header_length - 4, 4);
			else
				memset(reader->frame.mask, 0, 4);
			return true;
		}
	}

	return false;
}

int main(int argc, char **argv) {
	InitThreadContext(KiloBytes(64));
	ThreadContextSetLogger({ LogProcedure, nullptr });

	if (argc != 2) {
		fprintf(stderr, "USAGE: %s token\n\n", argv[0]);
		return 1;
	}

	Net_Initialize();

	srand((uint32_t)time(nullptr));

	Memory_Arena *arenas[2];
	arenas[0] = MemoryArenaAllocate(MegaBytes(64));
	arenas[1] = MemoryArenaAllocate(MegaBytes(64));

	Net_Socket *http = Http_Connect("https://discord.com");
	if (!http) {
		return 1;
	}

	const String token = FmtStr(ThreadScratchpad(), "Bot %s", argv[1]);

	Http_Request *req = Http_CreateRequest(http, arenas[0]);
	Http_SetHeader(req, HTTP_HEADER_AUTHORIZATION, token);
	Http_SetHeader(req, HTTP_HEADER_USER_AGENT, "Katachi");
	Http_SetHeader(req, HTTP_HEADER_CONNECTION, "keep-alive");
	//Http_RequestSetContent(req, "application/json", Message);

	Http_Body_Writer writer = { Dump };

	uint8_t read_buffer[100] = {};
	int bytes_sent = Net_ReceiveBlocked(http, read_buffer, sizeof(read_buffer));

	Http_Response *res = Http_Get(http, "/api/v9/gateway/bot", req);
	
	if (res) {
		printf("%.*s\n\n", (int)res->body.length, res->body.data);

		Http_DestroyResponse(res);
	}

	Http_Disconnect(http);

	// wss://gateway.discord.gg/?v=9&encoding=json
	
	Net_Socket *websocket = Http_Connect("wss://gateway.discord.gg", HTTPS_CONNECTION);
	if (!websocket) {
		return 1;
	}

	uint8_t buffer[SecureWebSocketKeySize];
	String wsskey = GenerateSecureWebSocketKey(buffer);

	req = Http_CreateRequest(websocket, arenas[0]);
	Http_SetHeader(req, HTTP_HEADER_AUTHORIZATION, token);
	Http_SetHeader(req, HTTP_HEADER_USER_AGENT, "Katachi");
	Http_SetHeader(req, HTTP_HEADER_UPGRADE, "websocket");
	Http_SetHeader(req, HTTP_HEADER_CONNECTION, "Upgrade");
	Http_SetCustomHeader(req, "Sec-WebSocket-Version", "13");
	Http_SetCustomHeader(req, "Sec-WebSocket-Key", wsskey);

	res = Http_Get(websocket, "/?v=9&encoding=json", req);

	if (res) {
		if (res->status.code == 101) {
			printf("%.*s\n\n", (int)res->body.length, res->body.data);

			String given = Http_GetCustomHeader(res, "Sec-WebSocket-Accept");
			if (!given.length) return 1;

			constexpr char WebsocketKeySalt[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
			constexpr int WebsocketKeySaltSize = sizeof(WebsocketKeySalt) - 1;

			uint8_t combined[SecureWebSocketKeySize + WebsocketKeySaltSize];
			memcpy(combined, wsskey.data, SecureWebSocketKeySize);
			memcpy(combined + SecureWebSocketKeySize, WebsocketKeySalt, WebsocketKeySaltSize);

			uint8_t digest[20];
			SHA1((char *)digest, (char *)combined, SecureWebSocketKeySize + WebsocketKeySaltSize);

			uint8_t buffer_base64[Base64EncodedSize(20)];
			String required = EncodeBase64(String(digest, sizeof(digest)), buffer_base64, sizeof(buffer_base64));

			if (required != given) {
				WriteLogErrorEx("WSS", "Server returned invalid accept value");
			} else {
				WriteLogInfo("Websocket connected.");
			}

			Http_DestroyResponse(res);

			Websocket_Frame_Writer writer;
			Websocket_FrameWriterInit(&writer, arenas[0]);

			Websocket_Frame_Reader reader;
			Websocket_FrameReaderInit(&reader, arenas[1]);

			int timeout = NET_TIMEOUT_MILLISECS;

			bool connected = true;
			while (connected) {
				pollfd fd;
				fd.fd = Net_GetSocketDescriptor(websocket);
				fd.revents = 0;

				fd.events = POLLRDNORM;
				if (writer.first)
					fd.events |= POLLWRNORM;

				int presult = poll(&fd, 1, timeout);
				if (presult < 0) break;

				if (presult > 0) {
					// TODO: Loop until we can no longer sent data
					if (fd.revents & POLLWRNORM) {
						Assert(writer.first);

						int bytes_sent = Net_Send(websocket, writer.first->buffer + writer.first->sent, writer.first->sent - writer.first->length);
						if (bytes_sent < 0) break;
						writer.first->length += bytes_sent;

						if (writer.first->sent == WEBSOCKET_STREAM_SIZE) {
							auto free_buk = writer.first;
							writer.first = free_buk->next;
							free_buk->next = writer.free_list;
							free_buk->prev = nullptr;
							writer.free_list = free_buk;
							writer.free_count += 1;
						}
					}

					// TODO: Loop until we can't reveive data
					if (fd.revents & POLLRDNORM) {
						int bytes_received = Net_Receive(websocket, reader.buffer + reader.length, (int)(reader.allocated - reader.length));
						if (bytes_received < 0) break;
						reader.length += bytes_received;

						if (Websocket_FrameReaderUpdate(&reader)) {
							if (reader.frame.masked) break; // servers don't mask
							if (reader.frame.opcode & 0x80) {
								if (reader.frame.payload_length > 125) break; // not allowed by specs
								// TODO: handle control frames
								WriteLogInfoEx("Websocket", "control frame");
							} else {

								String payload(reader.frame.payload, reader.frame.payload_length);

								Json json_event;
								if (JsonParse(payload, &json_event, MemoryArenaAllocator(reader.arena))) {

								}
							}

							MemoryArenaReset(reader.arena);
							Websocket_FrameReaderInit(&reader, reader.arena);
						}
					}
				}

				if (presult == 0) {

				}
			}


#if 0
			Websocket_Result result = OK;

#if 0
			constexpr int STREAM_SIZE = KiloBytes(8);

			uint8_t stream[STREAM_SIZE] = {};

			int bytes_received = Websocket_ReceiveMinimum(websocket, stream, STREAM_SIZE, 2);

			int fin = (stream[0] & 0x80) >> 7;
			int rsv = (stream[0] & 0x70) >> 4; // must be zero, since we don't use extensions
			int opcode = (stream[0] & 0x0f);
			int mask = (stream[1] & 0x80) >> 7; // must be zero for client, must be 1 when sending data to server

			uint64_t payload_len = (stream[1] & 0x7f);

			int consumed_size = 2;

			if (payload_len == 126) {
				if (bytes_received < 4) {
					bytes_received += Websocket_ReceiveMinimum(websocket, stream + bytes_received, STREAM_SIZE - bytes_received, 4 - bytes_received);
				}

				union Payload_Length2 {
					uint8_t  parts[2];
					uint16_t combined;
				};

				Payload_Length2 length;
				length.parts[0] = stream[3];
				length.parts[1] = stream[2];

				consumed_size = 4;
				payload_len = length.combined;
			} else if (payload_len == 127) {
				if (bytes_received < 10) {
					bytes_received += Websocket_ReceiveMinimum(websocket, stream + bytes_received, STREAM_SIZE - bytes_received, 4 - bytes_received);
				}

				union Payload_Length8 {
					uint8_t  parts[8];
					uint64_t combined;
				};

				Payload_Length8 length;
				length.parts[0] = stream[9];
				length.parts[1] = stream[8];
				length.parts[2] = stream[7];
				length.parts[3] = stream[6];
				length.parts[4] = stream[5];
				length.parts[5] = stream[4];
				length.parts[6] = stream[3];
				length.parts[7] = stream[2];

				consumed_size = 10;
				payload_len = length.combined;
			}

			uint8_t *payload_buff = (uint8_t *)PushSize(arena, payload_len);

			int payload_read = bytes_received - consumed_size;
			memcpy(payload_buff, stream + consumed_size, payload_read);

			uint8_t *read_ptr = payload_buff + payload_read;
			uint64_t remaining = payload_len - payload_read;
			while (remaining) {
				int bytes_read = Net_Read(websocket, read_ptr, (int)remaining);
				Assert(bytes_read >= 0);
				read_ptr += bytes_read;
				remaining -= bytes_read;
			}

			String payload(payload_buff, payload_len);
#else
			Websocket_Frame frame;
			result = Websocket_Receive(websocket, arena, &frame);
			Assert(result == OK);

			String payload(frame.payload, (ptrdiff_t)frame.payload_length);
#endif


			Json json;
			if (JsonParse(payload, &json, MemoryArenaAllocator(arena))) {
				WriteLogInfo("Got JSON");
			}

			Assert(json.type == JSON_TYPE_OBJECT);

			Json *d = JsonObjectFind(&json.value.object, "d");
			Assert(d && d->type == JSON_TYPE_OBJECT);

			Json *hi = JsonObjectFind(&d->value.object, "heartbeat_interval");
			Assert(hi && hi->type == JSON_TYPE_NUMBER);

			int heartbeat_interval = hi->value.number.value.integer;

			MemoryArenaReset(arena);

			String_Builder builder;
			builder.allocator = MemoryArenaAllocator(arena);

			uint8_t header[8];
			memset(header, 0, sizeof(header));
			WriteBuffer(&builder, header, sizeof(header));

			Json_Builder identity = JsonBuilderCreate(&builder);

			JsonWriteBeginObject(&identity);
			JsonWriteKeyNumber(&identity, "op", 2);
			JsonWriteKey(&identity, "d");
			JsonWriteBeginObject(&identity);
			JsonWriteKeyString(&identity, "token", String(argv[1], strlen(argv[1])));
			JsonWriteKeyNumber(&identity, "intents", 513);
			JsonWriteKey(&identity, "properties");
			JsonWriteBeginObject(&identity);
			JsonWriteKeyString(&identity, "$os", "windows");
			JsonWriteEndObject(&identity);
			JsonWriteEndObject(&identity);
			JsonWriteEndObject(&identity);

			payload = BuildString(&builder, builder.allocator);

#if 0
			header[0] |= 0x80; // fin
			header[0] |= OP_TEXT_FRAME;
			header[1] |= 0x80;
			header[1] |= 126;

			union Payload_Length2 {
				uint8_t  parts[2];
				uint16_t combined;
			};

			Payload_Length2 len2;
			len2.combined = (uint16_t)payload.length - sizeof(header);
			header[2] = len2.parts[1];
			header[3] = len2.parts[0];

			//uint32_t mask_value = (uint32_t)rand();
			//uint8_t *mask_ptr = (uint8_t *)&mask_value;

			uint8_t mask_se[4] = { 50, 76, 88, 100 };

			header[4] = mask_se[0];
			header[5] = mask_se[1];
			header[6] = mask_se[2];
			header[7] = mask_se[3];

			Websocket_MaskPayload(payload.data + sizeof(header), payload.data + sizeof(header), payload.length - sizeof(header), mask_se);
			//Websocket_MaskPayload(payload.data + sizeof(header), payload.length - sizeof(header), mask_se);

			char *thing = (char *)payload.data + 8;

			memcpy(payload.data, header, sizeof(header));

			int bytes_written = 0;
			//bytes_written += Net_Write(websocket, header, sizeof(header));
			bytes_written += Net_Write(websocket, payload.data, (int)payload.length);
#else
			result = Websocket_SendText(websocket, payload.data, payload.length, arena);
#endif

			while (true) {
				//result = Websocket_Receive(websocket, arena, &frame);

				int fuck_visual_studio = 42;
			}


			int fuck_visual_studio = 42;
		}
#endif
		}
	}

	Http_Disconnect(websocket);

	Net_Shutdown();

	return 0;
}
