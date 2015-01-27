/*
 * packet.h
 *
 *  Created on: 2014-8-22
 *      Author: Administrator
 */

#ifndef PACKET_H_
#define PACKET_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#define HEAD_UNPACK_FORMAT "%[^,],%ld,%[^,]"
#define HEAD_PACK_FORMAT "%s,%ld,%s"

#define HEAD_PACKET_SIZE 300

#define BODY_PACKET_SIZE 409600

typedef struct {
	char command[50];
	long contentLen;
	char extraInfo[200];
} head_packet_t;

#define head_packet_pack(data, head_packet) pack(data, head_packet.command, head_packet.contentLen, head_packet.extraInfo);

//打包
void pack(char *data,...);

//解包
void unpack(char *data, head_packet_t *head_packet);

#endif /* PACKET_H_ */
