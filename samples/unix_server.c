#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>

#define BACK_LOG 1000
#define BUFFER_SIZE 1024

char buffer[BUFFER_SIZE] = {0};
char path[] = "/tmp/neo.sock";

void handleError(char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

void bindToAddress(int serverSocket) {
	struct sockaddr_un address;
	address.sun_family = PF_UNIX;
	strncpy(address.sun_path, path, sizeof(path));

	if (remove(path) == -1 && errno != ENOENT)
		handleError("删除失败");
	if (bind(serverSocket, (struct sockaddr *)&address, sizeof(address)) == -1)
		handleError("地址绑定失败");
}

void echo(int socket) {
	int numberOfReaded, numberOfWrited = 0;
	while (true) {
		numberOfReaded = recv(socket, buffer, BUFFER_SIZE, 0);
		if (numberOfReaded == -1)
			handleError("读取数据错误");
		else if (numberOfReaded == 0) {
			printf("客户端关闭连接\n");
			close(socket);
			return;
		}
		printf("Echoing ...", numberOfReaded);
		if (numberOfReaded > 0) {
			numberOfWrited = write(socket, buffer, numberOfReaded);
			printf("  写入的结果为%d\n", numberOfWrited);
		}
	}
}

void handleRequest(int serverSocket) {
	int socket = accept(serverSocket, NULL, NULL);
	if (socket == -1)
		handleError("accept 错误");
	puts("client发起连接...");
	echo(socket);
}

int main(void) {
	int serverSocket = socket(PF_UNIX, SOCK_STREAM, 0);
	if (serverSocket == -1)
		handleError("创建socket失败");

	bindToAddress(serverSocket);
	if (listen(serverSocket, BACK_LOG) == -1)
		handleError("监听失败");

	while (true)
		handleRequest(serverSocket);
	return EXIT_SUCCESS;
}
