#include "KrCommon.h"

struct Net_Socket {
    int64_t descriptor;
	void *handle;

    struct {
	    char hostname[256];
        uint16_t hostname_length;
	    char port[8];
    } info;
};

enum Net_Result {
    Net_Ok,
	Net_Error, // TODO: Better error message
};

Net_Result NetInit();
void NetShutdown();

Net_Result NetOpenClientConnection(const String hostname, const String port, Net_Socket *net);
Net_Result NetPerformTSLHandshake(Net_Socket *net);
void NetCloseConnection(Net_Socket *net);

int NetWrite(Net_Socket *net, void *buffer, int length);
int NetRead(Net_Socket *net, void *buffer, int length);
int NetWriteSecured(Net_Socket *net, void *buffer, int length);
int NetReadSecured(Net_Socket *net, void *buffer, int length);
