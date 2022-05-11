#pragma once
#include "Http.h"

constexpr int WEBSOCKET_MAX_PROTOCOLS = 16;
constexpr int WEBSOCKET_MAX_WAIT_MS   = 500;

struct Websocket_Procotols {
	ptrdiff_t count;
	String    data[WEBSOCKET_MAX_PROTOCOLS];
};

struct Websocket_Header {
	Http_Header         headers;
	Websocket_Procotols protocols;
};

void Websocket_InitHeader(Websocket_Header *header);
void Websocket_HeaderAddProcotols(Websocket_Header *header, String protocol);
void Websocket_HeaderSet(Websocket_Header *header, Http_Header_Id id, String value);
void Websocket_HeaderSet(Websocket_Header *header, String name, String value);

struct Websocket;

struct Websocket_Spec {
	uint32_t read_size;
	uint32_t write_size;
	uint32_t queue_size;
};

constexpr Websocket_Spec WebsocketDefaultSpec = { KiloBytes(12), KiloBytes(12), 1024 };

Websocket *Websocket_Connect(String uri, Http_Response *res, Websocket_Header *header = nullptr, Websocket_Spec spec = WebsocketDefaultSpec, Memory_Allocator allocator = ThreadContext.allocator);
void       Websocket_Disconnect(Websocket *websocket);

//
//
//

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
	WEBSOCKET_E_WAIT,
	WEBSOCKET_E_NOMEM,
	WEBSOCKET_E_CLOSED,
	WEBSOCKET_E_SYSTEM,
};

enum Websocket_Event_Type {
	WEBSOCKET_EVENT_TEXT,
	WEBSOCKET_EVENT_BINARY,
	WEBSOCKET_EVENT_PING,
	WEBSOCKET_EVENT_PONG,
	WEBSOCKET_EVENT_CLOSE,
};

struct Websocket_Event {
	Websocket_Event_Type type;
	Buffer               message;
};

enum Websocket_Close_Reason {
	WEBSOCKET_CLOSE_NORMAL = 1000,
	WEBSOCKET_CLOSE_GOING_AWAY = 1001,
	WEBSOCKET_CLOSE_PROTOCOL_ERROR = 1002,
	WEBSOCKET_CLOSE_UNSUPPORTED_DATA = 1003,
	WEBSOCKET_CLOSE_NOT_RECEIVED = 1005,
	WEBSOCKET_CLOSE_ABNORMAL_CLOSURE = 1006,
	WEBSOCKET_CLOSE_INVALID_FRAME_PAYLOAD_DATA = 1007,
	WEBSOCKET_CLOSE_POLICY_VOILATION = 1008,
	WEBSOCKET_CLOSE_MESSAGE_TOO_BIG = 1009,
	WEBSOCKET_CLOSE_MANDATORY_EXTENSION = 1010,
	WEBSOCKET_CLOSE_INTERNAL_SERVER_ERROR = 1011,
	WEBSOCKET_CLOSE_TLS_HANDSHAKE = 1012
};

bool Websocket_IsConnected(Websocket *websocket);
ptrdiff_t Websocket_GetFrameSize(Websocket *websocket, ptrdiff_t payload_len);
Websocket_Result Websocket_Send(Websocket *websocket, String raw_data, Websocket_Opcode opcode, int timeout);
Websocket_Result Websocket_SendText(Websocket *websocket, String raw_data, int timeout);
Websocket_Result Websocket_SendBinary(Websocket *websocket, String raw_data, int timeout);
Websocket_Result Websocket_Ping(Websocket *websocket, String raw_data, int timeout);
Websocket_Result Websocket_Close(Websocket *websocket, Websocket_Close_Reason reason, int timeout);
Websocket_Result Websocket_Receive(Websocket *websocket, Websocket_Event *event, ptrdiff_t bufflen, int timeout);
