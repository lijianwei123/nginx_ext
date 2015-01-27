#include "packet.h"

//打包
void pack(char *data,...)
{
	va_list args;
	va_start(args, data);
	vsprintf(data, HEAD_PACK_FORMAT, args);
	va_end(args);
}

//解包
void unpack(char *data, head_packet_t *head_packet)
{
	sscanf(data, HEAD_UNPACK_FORMAT, head_packet->command, &head_packet->contentLen, head_packet->extraInfo);
}

