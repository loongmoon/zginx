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
#define	MAXLINE	 8192
#define SERVER_STRING "Server: zginx/0.1.0\r\n"

extern int errno;
char* cwd;

#define sendmsg(fd, buf) send(fd, buf, strlen(buf), 0)
void *request_handle(void *param);
static int parse_uri(char *uri, char *filename, char *cgiargs);
int read_line(int, char *, int);
void unimplemented(int); 
void static_file(int, const char *);
void not_found(int);
void headers(int, const char *); 
void copy_file(int, FILE *);
void exec_cgi(int, const char *, const char *, const char *);
void bad_request(int);
void cannot_execute(int);

typedef struct request_s {
	struct sockaddr_in client_addr;
	socklen_t          client_addr_len;
	int                client_fd;
	pthread_t          thread_id;
} request_t;

int main()
{
	printf("zginx正在%d端口进行监听", PORT);
	int server_fd = -1;
	int client_fd = -1;
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char buf[BUFF_SIZE];
	getcwd(cwd, sizeof(cwd));

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
	int numchars;  
	char method[255]; 
	size_t i, j;  
	char url[255];
	char path[512];
	struct stat st;
	int cgi = 0;
	char *query_string = NULL; 

	//ssize_t num = read(req->client_fd, buf, sizeof(buf));
	//printf("Receive from %s:%d with %lu bytes\n", inet_ntoa(req->client_addr.sin_addr), req->client_addr.sin_port,
	//			num);

	/*得到请求的第一行*/  
	numchars = read_line(req->client_fd, buf, sizeof(buf));  
	i = 0; j = 0;  
	/*把客户端的请求方法存到 method 数组*/  
	while (!isspace(buf[j]) && (i < sizeof(method) - 1))  
	{  
		method[i] = buf[j];  
		i++; j++;  
	}  
	method[i] = '\0'; 
	/*如果既不是 GET 又不是 POST 则无法处理 */  
	if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))  
	{  
		unimplemented(req->client_fd);  
		return NULL;  
	}  

	/* POST 的时候开启 cgi */  
	if (strcasecmp(method, "POST") == 0)  
		cgi = 1;  

	/*读取 url 地址*/  
	i = 0;  
	while (isspace(buf[j]) && (j < sizeof(buf)))  
		j++;  
	while (!isspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))  
	{  
		/*存下 url */  
		url[i] = buf[j];  
		i++; j++;  
	}  
	url[i] = '\0'; 
	printf("url:%s", url);

	/*处理 GET 方法*/  
	if (strcasecmp(method, "GET") == 0)  
	{  
		/* 待处理请求为 url */  
		query_string = url;  
		while ((*query_string != '?') && (*query_string != '\0'))  
			query_string++;  
		/* GET 方法特点，? 后面为参数*/  
		if (*query_string == '?')  
		{  
			/*开启 cgi */  
			cgi = 1;  
			*query_string = '\0';  
			query_string++;  
		}  
	}  
	/*格式化 url 到 path 数组，html 文件都在 htdocs 中*/  
	sprintf(path, "htdocs%s", url);  
	/*默认情况为 index.html */  
	if (path[strlen(path) - 1] == '/')  
		strcat(path, "index.html");  
	/*根据路径找到对应文件 */  
	if (stat(path, &st) == -1) {  
		/*把所有 headers 的信息都丢弃*/  
		while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */  
			numchars = read_line(req->client_fd, buf, sizeof(buf));  
		/*回应客户端找不到*/  
		not_found(req->client_fd);  
	}  
	else  
	{  
		/*如果是个目录，则默认使用该目录下 index.html 文件*/  
		if ((st.st_mode & S_IFMT) == S_IFDIR)  
			strcat(path, "/index.html");  
		if ((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)    )  
			cgi = 1;  
		/*不是 cgi,直接把服务器文件返回，否则执行 cgi */  
		if (!cgi)  
			static_file(req->client_fd, path);  
		else  
			exec_cgi(req->client_fd, path, method, query_string);  
	} 

	memset(buf, 0, 1024);

	//sprintf(buf, "HTTP/1.0 200 OK\r\nServer: zginx/0.1.0\r\nContent-Type: text/html\r\nContent-Length: 7\r\n\r\nzginx\r\n");
	//sendmsg(req->client_fd, buf);
	close(req->client_fd);

	free(req);

	return NULL;
}
int read_line(int sock, char *buf, int size)  
{  
	int i = 0;  
	char c = '\0';  
	int n;  

	/*把终止条件统一为 \n 换行符，标准化 buf 数组*/  
	while ((i < size - 1) && (c != '\n'))  
	{  
		/*一次仅接收一个字节*/  
		n = recv(sock, &c, 1, 0);  
		/* DEBUG printf("%02X\n", c); */  
		if (n > 0)  
		{  
			/*收到 \r 则继续接收下个字节，因为换行符可能是 \r\n */  
			if (c == '\r')  
			{  
				/*使用 MSG_PEEK 标志使下一次读取依然可以得到这次读取的内容，可认为接收窗口不滑动*/  
				n = recv(sock, &c, 1, MSG_PEEK);  
				/* DEBUG printf("%02X\n", c); */  
				/*但如果是换行符则把它吸收掉*/  
				if ((n > 0) && (c == '\n'))  
					recv(sock, &c, 1, 0);  
				else  
					c = '\n';  
			}  
			/*存到缓冲区*/  
			buf[i] = c;  
			i++;  
		}  
		else  
			c = '\n';  
	}  
	buf[i] = '\0';  

	/*返回 buf 数组大小*/  
	return(i);  
}  
static int parse_uri(char *uri, char *filename, char *cgiargs)   
{  
	char *ptr;  
	char tmpcwd[MAXLINE];  
	strcpy(tmpcwd,cwd);  
	strcat(tmpcwd,"/");  

	if (!strstr(uri, "cgi-bin"))   
	{  /* Static content */  
		strcpy(cgiargs, "");  
		strcpy(filename, tmpcwd);  
		strcat(filename, uri);  
		if (uri[strlen(uri)-1] == '/')  
			strcat(filename, "home.html");  
		return 1;  
	}  
	else   
	{  /* Dynamic content */  
		ptr = index(uri, '?');  
		if (ptr)   
		{  
			strcpy(cgiargs, ptr+1);  
			*ptr = '\0';  
		}  
		else   
			strcpy(cgiargs, "");  
		strcpy(filename, cwd);  
		strcat(filename, uri);  
		return 0;  
	}  
} 
void unimplemented(int client)  
{  
	char buf[1024];  

	/* HTTP method 不被支持*/  
	sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");  
	send(client, buf, strlen(buf), 0);  
	/*服务器信息*/  
	sprintf(buf, SERVER_STRING);  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "Content-Type: text/html\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "</TITLE></HEAD>\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "</BODY></HTML>\r\n");  
	send(client, buf, strlen(buf), 0);  
}  
void static_file(int client, const char *filename)  
{  
	FILE *resource = NULL;  
	int numchars = 1;  
	char buf[1024];  

	/*读取并丢弃 header */  
	buf[0] = 'A'; buf[1] = '\0';  
	while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */  
		numchars = read_line(client, buf, sizeof(buf));  

	/*打开 sever 的文件*/  
	resource = fopen(filename, "r");  
	if (resource == NULL)  
		not_found(client);  
	else  
	{  
		/*写 HTTP header */  
		headers(client, filename);  
		/*复制文件*/  
		copy_file(client, resource);  
	}  
	fclose(resource);  
} 
void not_found(int client)  
{  
	char buf[1024];  

	/* 404 页面 */  
	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");  
	send(client, buf, strlen(buf), 0);  
	/*服务器信息*/  
	sprintf(buf, SERVER_STRING);  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "Content-Type: text/html\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "<BODY><P>The server could not fulfill\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "your request because the resource specified\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "is unavailable or nonexistent.\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "</BODY></HTML>\r\n");  
	send(client, buf, strlen(buf), 0);  
}  
void headers(int client, const char *filename)  
{  
	char buf[1024];  
	(void)filename;  /* could use filename to determine file type */  

	/*正常的 HTTP header */  
	strcpy(buf, "HTTP/1.0 200 OK\r\n");  
	send(client, buf, strlen(buf), 0);  
	/*服务器信息*/  
	strcpy(buf, SERVER_STRING);  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "Content-Type: text/html;charset=utf-8\r\n");  
	send(client, buf, strlen(buf), 0);  
	strcpy(buf, "\r\n");  
	send(client, buf, strlen(buf), 0);  
}  
void copy_file(int client, FILE *resource)  
{  
	char buf[1024];  

	/*读取文件中的所有数据写到 socket */  
	fgets(buf, sizeof(buf), resource);  
	while (!feof(resource))  
	{  
		send(client, buf, strlen(buf), 0);  
		fgets(buf, sizeof(buf), resource);  
	}  
}  
void exec_cgi(int client, const char *path, const char *method, const char *query_string)  
{  
	char buf[1024];  
	int cgi_output[2];  
	int cgi_input[2];  
	pid_t pid;  
	int status;  
	int i;  
	char c;  
	int numchars = 1;  
	int content_length = -1;  

	buf[0] = 'A'; buf[1] = '\0';  
	if (strcasecmp(method, "GET") == 0)  
		/*把所有的 HTTP header 读取并丢弃*/  
		while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */  
			numchars = read_line(client, buf, sizeof(buf));  
	else    /* POST */  
	{  
		/* 对 POST 的 HTTP 请求中找出 content_length */  
		numchars = read_line(client, buf, sizeof(buf));  
		while ((numchars > 0) && strcmp("\n", buf))  
		{  
			/*利用 \0 进行分隔 */  
			buf[15] = '\0';  
			/* HTTP 请求的特点*/  
			if (strcasecmp(buf, "Content-Length:") == 0)  
				content_length = atoi(&(buf[16]));  
			numchars = read_line(client, buf, sizeof(buf));  
		}  
		/*没有找到 content_length */  
		if (content_length == -1) {  
			/*错误请求*/  
			bad_request(client);  
			return;  
		}  
	}  

	/* 正确，HTTP 状态码 200 */  
	sprintf(buf, "HTTP/1.0 200 OK\r\n");  
	send(client, buf, strlen(buf), 0);  

	/* 建立管道*/  
	if (pipe(cgi_output) < 0) {  
		/*错误处理*/  
		cannot_execute(client);  
		return;  
	}  
	/*建立管道*/  
	if (pipe(cgi_input) < 0) {  
		/*错误处理*/  
		cannot_execute(client);  
		return;  
	}  

	if ((pid = fork()) < 0 ) {  
		/*错误处理*/  
		cannot_execute(client);  
		return;  
	}  
	if (pid == 0)  /* child: CGI script */  
	{  
		char meth_env[255];  
		char query_env[255];  
		char length_env[255];  

		/* 把 STDOUT 重定向到 cgi_output 的写入端 */  
		dup2(cgi_output[1], 1);  
		/* 把 STDIN 重定向到 cgi_input 的读取端 */  
		dup2(cgi_input[0], 0);  
		/* 关闭 cgi_input 的写入端 和 cgi_output 的读取端 */  
		close(cgi_output[0]);  
		close(cgi_input[1]);  
		/*设置 request_method 的环境变量*/  
		sprintf(meth_env, "REQUEST_METHOD=%s", method);  
		putenv(meth_env);  
		if (strcasecmp(method, "GET") == 0) {  
			/*设置 query_string 的环境变量*/  
			sprintf(query_env, "QUERY_STRING=%s", query_string);  
			putenv(query_env);  
		}  
		else {   /* POST */  
			/*设置 content_length 的环境变量*/  
			sprintf(length_env, "CONTENT_LENGTH=%d", content_length);  
			putenv(length_env);  
		}  
		/*用 execl 运行 cgi 程序*/  
		execl(path, path, NULL);  
		exit(0);  
	} else {    /* parent */  
		/* 关闭 cgi_input 的读取端 和 cgi_output 的写入端 */  
		close(cgi_output[1]);  
		close(cgi_input[0]);  
		if (strcasecmp(method, "POST") == 0)  
			/*接收 POST 过来的数据*/  
			for (i = 0; i < content_length; i++) {  
				recv(client, &c, 1, 0);  
				/*把 POST 数据写入 cgi_input，现在重定向到 STDIN */  
				write(cgi_input[1], &c, 1);  
			}  
		/*读取 cgi_output 的管道输出到客户端，该管道输入是 STDOUT */  
		while (read(cgi_output[0], &c, 1) > 0)  
			send(client, &c, 1, 0);  

		/*关闭管道*/  
		close(cgi_output[0]);  
		close(cgi_input[1]);  
		/*等待子进程*/  
		waitpid(pid, &status, 0);  
	}  
} 
void bad_request(int client)  
{  
	char buf[1024];  

	/*回应客户端错误的 HTTP 请求 */  
	sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");  
	send(client, buf, sizeof(buf), 0);  
	sprintf(buf, "Content-type: text/html\r\n");  
	send(client, buf, sizeof(buf), 0);  
	sprintf(buf, "\r\n");  
	send(client, buf, sizeof(buf), 0);  
	sprintf(buf, "<P>Your browser sent a bad request, ");  
	send(client, buf, sizeof(buf), 0);  
	sprintf(buf, "such as a POST without a Content-Length.\r\n");  
	send(client, buf, sizeof(buf), 0);  
} 
void cannot_execute(int client)  
{  
	char buf[1024];  

	/* 回应客户端 cgi 无法执行*/  
	sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "Content-type: text/html\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "\r\n");  
	send(client, buf, strlen(buf), 0);  
	sprintf(buf, "<P>Error prohibited CGI execution.\r\n");  
	send(client, buf, strlen(buf), 0);  
}  
