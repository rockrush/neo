#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

char buffer[BUFFER_SIZE] = {0};
char path[] = "/tmp/neo.sock";

void handleError(char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(void) {
	int client = socket(PF_UNIX, SOCK_STREAM, 0);
	if (client == -1)
		handleError("创建socket失败");

	struct sockaddr_un addr;
	addr.sun_family = PF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));

	if (connect(client, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		handleError("连接服务端失败");

	while(true) {
		fgets(buffer, BUFFER_SIZE, stdin);
		if(send(client, buffer, strlen(buffer), 0)==-1)
            handleError("发送失败");
		int read_cnt = recv(client, buffer, BUFFER_SIZE, 0);
		if(read_cnt == -1)
            handleError("对端已经关闭");
		buffer[read_cnt]=0;
		printf("%s", buffer);
	}
}
