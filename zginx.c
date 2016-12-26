#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define PORT 9001
#define QUEUE_MAX_COUNT 5
#define BUFF_SIZE 1024
#define SERVER_STRING "Server: zginx/0.1.0\r\n"
int main()
{
    int server_fd = -1;
    int client_fd = -1;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buf[BUFF_SIZE];
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(server_fd, (struct sockaddr *)&server_addr,sizeof(server_addr));
    listen(server_fd, QUEUE_MAX_COUNT);
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr,&client_addr_len);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client_fd, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client_fd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client_fd, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client_fd, buf, strlen(buf), 0);
    sprintf(buf, "zginx\r\n");
    send(client_fd, buf, strlen(buf), 0);
    close(client_fd);
    close(server_fd);

    return 0;
}
