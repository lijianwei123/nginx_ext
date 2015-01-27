#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sendfile.h>

#include "packet.h"

#define SERVER_IP "168.192.122.31"
#define SERVER_PORT 11011


int main(int argc, char *argv[])
{
	char packData[HEAD_PACKET_SIZE];
	head_packet_t head_packet;
	int contentLen = 0;
	char *filename = NULL;

	int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (clientSocket < 0) {
		perror("create socket error");
		goto finish;
	}

	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serverAddr.sin_port = htons(SERVER_PORT);

	if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in)) < 0) {
		perror("connect error");
		goto finish;
	}

	//先取head packet
	memset(packData, 0, HEAD_PACKET_SIZE);
	memset(&head_packet, 0, sizeof(head_packet_t));

	if (recv(clientSocket, packData, HEAD_PACKET_SIZE, 0) < 0) {
		perror("recv head packet error");
		goto finish;
	}
	unpack(packData, &head_packet);

	if (strcmp(head_packet.command, "send_filename") == 0) {
		contentLen = head_packet.contentLen;
		filename = head_packet.extraInfo;
	}

	//@see  http://man7.org/linux/man-pages/man2/open.2.html
	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
	int recvByteLen = 0;
	char recvBytes[BODY_PACKET_SIZE];
	int i = 0;
	for (i = contentLen; i > 0;) {
		memset(recvBytes, 0, BODY_PACKET_SIZE);

		recvByteLen = recv(clientSocket, recvBytes, BODY_PACKET_SIZE, 0);
		if (recvByteLen < 0) {
			perror("recv bytes error");
			close(fd);
			goto finish;
		}
		i = i - recvByteLen;

		write(fd, recvBytes, recvByteLen);
	}
	close(fd);


	finish:
		close(clientSocket);

	return 0;
}
