#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sendfile.h>


#include "packet.h"

#define SEND_PORT 11011

//文件存在
int file_exist(const char *filename)
{
	return access(filename, F_OK);
}


int main(int argc, char *argv[])
{
	int on = 1;
	int off = 0;

	char send_filename[200];


	//服务端地址
	struct sockaddr_in sendAddr;

	//服务socket
	int sendSocket = socket(AF_INET, SOCK_STREAM, 0);
	int reuse = 1;

	if (sendSocket < 0) {
		perror("create socket error");
		exit(EXIT_FAILURE);
	}

	struct stat stat_buff;

	memset(send_filename, 0, sizeof(send_filename));

	if (argc > 1) {
		strcpy(send_filename, argv[1]);
	}

#ifdef DEBUG
	//printf 输出到缓冲区，只有程序关闭，或者fflush(stdout)  或者内容含有 \n \r 才会立刻输出到stdout
	printf("send_filename:%s\n", send_filename);
#endif

	int fd = open(send_filename, O_RDONLY);
	fstat(fd, &stat_buff);

	if (fd < 0) {
		perror("open file error");
		exit(EXIT_FAILURE);
	}

	//搞定服务端地址
	memset(&sendAddr, 0, sizeof(sendAddr));
	sendAddr.sin_family = AF_INET;
	sendAddr.sin_addr.s_addr = inet_addr("168.192.122.31");
	sendAddr.sin_port = htons(SEND_PORT);

	setsockopt(sendSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (bind(sendSocket, (struct sockaddr *)&sendAddr, sizeof(struct sockaddr_in)) < 0) {
		perror("bind error");
		exit(1);
	}
	if (listen(sendSocket, 5)  < 0) {
		perror("listen error");
		exit(1);
	}

	//客户端地址
	struct sockaddr_in receiveAddr;
	socklen_t receiveAddrlen  = sizeof(struct sockaddr_in);

	int receiveSocket = 0;
	off_t offset;
	long haveSendBytes = 0;
	head_packet_t head_packet;
	char packData[HEAD_PACKET_SIZE];
	int sendByteLen = 0;

	while (1) {
		receiveSocket = accept(sendSocket, (struct sockaddr *)&receiveAddr, &receiveAddrlen);
		if (receiveSocket < 0) {
			perror("accept error");
			goto finish;
		}

		//发送head packet
		memset(&head_packet, 0, sizeof(head_packet_t));
		memset(packData, 0, HEAD_PACKET_SIZE);

		sprintf(head_packet.command, "%s", "send_filename");
		head_packet.contentLen = stat_buff.st_size;
		sprintf(head_packet.extraInfo, "%s", send_filename);

		pack(packData, head_packet.command, head_packet.contentLen, head_packet.extraInfo);
#ifdef DEBUG
		printf("packData:%s, size:%d\n", packData, sizeof(packData));
#endif

		setsockopt(receiveSocket, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
		sendByteLen = send(receiveSocket, packData, sizeof(packData), 0);

#ifdef DEBUG
		printf("sendByteLen:%d\n", sendByteLen);
#endif
		if (sendByteLen < 0) {
			perror("send head packet error");
			goto finish;
		}
		setsockopt(receiveSocket, IPPROTO_TCP, TCP_NODELAY, &off, sizeof(off));


		setsockopt(receiveSocket, IPPROTO_TCP, TCP_CORK, &on, sizeof(on));
		offset = 0;
		haveSendBytes = sendfile(receiveSocket, fd, &offset, stat_buff.st_size);
		setsockopt(receiveSocket, IPPROTO_TCP, TCP_CORK, &off, sizeof(off));

		if (haveSendBytes < 0) {
			perror("send file error");
			goto finish;
		}

#ifdef DEBUG
		printf("send bytes: %ld, offset: %ld\n", haveSendBytes, offset);
#endif

		finish:
			close(receiveSocket);
	}

	close(fd);


	return 0;
}
