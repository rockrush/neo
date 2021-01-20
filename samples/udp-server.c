#include <unistd.h> 
#include <arpa/inet.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <stdint.h> 
#include <string.h> 
  

#define MAXSIZE 8192 

int main(int argc, char *argv[])
{
	int sockfd;
	int n = 0; 
	char data[MAXSIZE]; 
	struct sockaddr_in6 addr, cli_addr;
	socklen_t addr_len = sizeof(struct sockaddr_in6); 
	uint16_t node_port = 2048;

	if ((sockfd = socket(PF_INET6, SOCK_DGRAM, 0)) == -1) {
		printf("Failed creating socket\n");
		return -1;
	}

	bzero(&addr, sizeof(addr));
	addr.sin6_family = PF_INET6;
	addr.sin6_port = htons(node_port);
	addr.sin6_addr = in6addr_any;

	if (bind(sockfd, (struct sockaddr *)&addr, addr_len) == -1) { 
		printf("Error in binding, already running ?\n");
		return -1;
	}

	while(1) { 
		printf("Waiting for data...\n"); 
		if((n = recvfrom(sockfd, data, MAXSIZE, 0, (struct sockaddr *)&cli_addr, &addr_len)) == -1) {
			printf("Error in receiving\n");
			exit(-1);
		}
		data[n] = '\0';

		printf("Received: %s\n", data); 
  
		//存储用"冒号十六进制记法"表示的IPv6地址 
		char buf_addr[40]; 
		//将IPv6地址转为"冒号十六进制记法"(colon hexadecimal notation)表示 
		inet_ntop(AF_INET6, &cli_addr.sin6_addr, buf_addr, 64); 
  
		printf("Client ip: %s\n", buf_addr); 
		printf("Client port: %d\n", ntohs(cli_addr.sin6_port));
		printf("\n"); 
	} 

	close(sockfd);
	return -1;
}
