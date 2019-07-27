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
	int n = 10; 
	char *data = "keepalive";
	struct sockaddr_in6 addr;
	uint16_t node_port = 2048;

	if ((sockfd = socket(PF_INET6, SOCK_DGRAM, 0)) == -1) {
		printf("Failed creating socket\n");
		return -1;
	}
	if (argc > 1) {
		data = argv[1];
		n = strlen(data) + 1;
	}

	bzero(&addr, sizeof(addr));
	addr.sin6_family = PF_INET6;
	addr.sin6_port = htons(node_port);
	addr.sin6_addr = in6addr_loopback;

	printf("Sending data...\n"); 
	sendto(sockfd, data, n, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in6));
  
	close(sockfd);
	return -1;
}
