#include "Http.h"

static constexpr int WEBSOCKET_MAX_PROTOCOLS = 16;

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

enum Websocket_Opcode {
	WEBSOCKET_OP_CONTINUATION_FRAME = 0x0,
	WEBSOCKET_OP_TEXT_FRAME = 0x1,
	WEBSOCKET_OP_BINARY_FRAME = 0x2,
	WEBSOCKET_OP_CONNECTION_CLOSE = 0x8,
	WEBSOCKET_OP_PING = 0x9,
	WEBSOCKET_OP_PONG = 0xa
};

enum Websocket_Close_Reason {
	WEBSOCKET_CLOSE_NORMAL                     = 1000,
	WEBSOCKET_CLOSE_GOING_AWAY                 = 1001,
	WEBSOCKET_CLOSE_PROTOCOL_ERROR             = 1002,
	WEBSOCKET_CLOSE_UNSUPPORTED_DATA           = 1003,
	WEBSOCKET_CLOSE_NOT_RECEIVED               = 1005,
	WEBSOCKET_CLOSE_ABNORMAL_CLOSURE           = 1006,
	WEBSOCKET_CLOSE_INVALID_FRAME_PAYLOAD_DATA = 1007,
	WEBSOCKET_CLOSE_POLICY_VOILATION           = 1008,
	WEBSOCKET_CLOSE_MESSAGE_TOO_BIG            = 1009,
	WEBSOCKET_CLOSE_MANDATORY_EXTENSION        = 1010,
	WEBSOCKET_CLOSE_INTERNAL_SERVER_ERROR      = 1011,
	WEBSOCKET_CLOSE_TLS_HANDSHAKE              = 1012
};

struct Websocket;

enum Websocket_Event_Type {
	WEBSOCKET_EVENT_PING,
	WEBSOCKET_EVENT_PONG,
	WEBSOCKET_EVENT_TEXT,
	WEBSOCKET_EVENT_BINARY,
	WEBSOCKET_EVENT_WRITE,
	WEBSOCKET_EVENT_CLOSE,
	WEBSOCKET_EVENT_TIMEOUT
};

struct Websocket_Event {
	Websocket_Event_Type type;
	Buffer message;
};

typedef void (*Websocket_On_Event_Proc)(Websocket *websocket, const Websocket_Event &event, void *user_context);

struct Websocket_Buffer_Size {
	ptrdiff_t read;
	ptrdiff_t write;
};

static constexpr Websocket_Buffer_Size WebsocketDefaultBufferSize = { 1024 * 64, 1024 * 64 };

void Websocket_OnMessageDump(Websocket *websocket, const Websocket_Event &event, void *user_context);

Websocket *      Websocket_Connect(String uri, Http_Response *res, Websocket_Header *header = nullptr, Websocket_Buffer_Size buffer_size = WebsocketDefaultBufferSize, Memory_Allocator allocator = ThreadContext.allocator);
void             Websocket_Disconnect(Websocket *websocket);

void      Websocket_SetTimeout(Websocket *websocket, int timeout);
ptrdiff_t Websocket_GetFrameSize(Websocket *websocket, ptrdiff_t payload_len);
bool      Websocket_IsWritable(Websocket *websocket);

enum Websocket_Result {
	WEBSOCKET_OK,
	WEBSOCKET_E_WAIT,
	WEBSOCKET_E_NOMEM,
	WEBSOCKET_E_CLOSED,
	WEBSOCKET_E_NETDOWN,
};

Websocket_Result Websocket_Send(Websocket *websocket, String raw_data, Websocket_Opcode opcode);
Websocket_Result Websocket_SendText(Websocket *websocket, String raw_data);
Websocket_Result Websocket_SendBinary(Websocket *websocket, String raw_data);

void Websocket_Ping(Websocket *websocket, String raw_data);
void Websocket_Pong(Websocket *websocket, String raw_data);
void Websocket_Close(Websocket *websocket, Websocket_Close_Reason reason);

void Websocket_Loop(Websocket *websocket, Websocket_On_Event_Proc event_proc, void *event_data);
