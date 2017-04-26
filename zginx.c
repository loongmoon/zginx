#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9002
#define QUEUE_MAX_COUNT 5
#define BUFF_SIZE 1024
#define SERVER_STRING "Server: zginx/0.1.0\r\n"


extern int errno;

#define sendmsg(fd, buf) send(fd, buf, strlen(buf), 0)


typedef struct request_s {
	struct sockaddr_in client_addr;
	socklen_t          client_addr_len;
	int                client_fd;
	pthread_t          thread_id;
} request_t;


void *request_handle(void *param);

int main()
{
	int server_fd = -1;
	int client_fd = -1;
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char buf[BUFF_SIZE];

	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	if(server_fd == -1) {
		perror("");
		exit(errno);
	} 

	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(server_fd, (struct sockaddr *)&server_addr,sizeof(server_addr)) == -1) {
		perror("");
		exit(errno);
	}

	if(listen(server_fd, QUEUE_MAX_COUNT) == -1) {
		perror("");
		exit(errno);
	}

	while(1) {
		request_t *request = malloc(sizeof(struct request_s));

		if((request->client_fd = accept(server_fd, (struct sockaddr *)&request->client_addr, 
						&request->client_addr_len)) == -1) {
			perror("");
			exit(errno);
		}
		//TODO

		if (pthread_create(&request->thread_id, NULL, request_handle, request) != 0){
			perror("");
			goto fail;
		}
	}

	close(server_fd);

	return 0;
fail:
	//TODO 
	return errno;
}

void *request_handle(void *param)
{
	request_t *req = (request_t*)param; 
	char buf[1024] = {0}; 

	ssize_t num = read(req->client_fd, buf, sizeof(buf));
	printf("Receive from %s:%d with %lu bytes\n", inet_ntoa(req->client_addr.sin_addr), req->client_addr.sin_port,
			num);

	memset(buf, 0, 1024);

	sprintf(buf, "HTTP/1.0 200 OK\r\nServer: zginx/0.1.0\r\nContent-Type: text/html\r\nContent-Length: 5\r\n\r\nzginx");
	sendmsg(req->client_fd, buf);
	close(req->client_fd);

	free(req);

	return NULL;
}
