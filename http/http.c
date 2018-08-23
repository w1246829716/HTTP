#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/wait.h>

#define HOME_PAGE "index.html"
#define MAX 1024
static void usage(const char *proc)
{
	printf("Usage:\n\t%s prot\n", proc);
}

int get_line(int sock, char line[], int num)
{
	assert(line);
	assert(num > 0);

	int i = 0;
	char c = 'x';
	while(c != '\n' && i < num-1)
	{
		ssize_t s = recv(sock, &c, 1, 0);
		if(s > 0)
		{
			if(c == '\r')
			{//\r or \r\n ----> \n
				recv(sock, &c, 1, MSG_PEEK);
				if(c == '\n')
					recv(sock, &c, 1, 0);
				else
					c = '\n';
			}
			line[i++] = c;
		}else{
			break;
		}
	}
	line[i] = 0;
	return i;
}

void clear_header(int sock)
{
	char line[MAX];
	do{
		get_line(sock, line, sizeof(line));
	}while(strcmp(line, "\n"));
}

void echo_error(int sock, int code)
{
	switch(code){
		case 404:
			//show_404();
			break;
		default:
			break;
	}
}

//
int echo_wwww(int sock, char *path, int size)
{
	int fd = open(path, O_RDONLY);//打开对应文件
	if(fd < 0)
	{
		return 404;
	}
	//清除header
	clear_header(sock);
	//给客户回复
	char line[MAX];
	sprintf(line, "HTTP/1.0 200 OK\r\n");//首行
	send(sock, line, strlen(line), 0);

	sprintf(line, "Content-Type: text/html;charset=ISO-8859-1\r\n");//请求头
	send(sock, line, strlen(line), 0);

	sprintf(line, "\r\n");//空行
	send(sock, line, strlen(line), 0);

	//发送正文
	sendfile(sock, fd, NULL, size);//拷贝数据，从一个描述符拷到另一个描述符，内核中拷贝。将fd文件描述符中的内容写到sock中。

	close(fd);
}

void *header_request(void *arg)
{
	int sock = (int)arg;
	char line[MAX] = {0};
	char method[MAX/10];//方法
	char url[MAX] = {0};//url请求
	char path[MAX] = {0};//用来保存路径, 也就是url前的资源路径字符串
	int i=0, j=0;
	int status_code = 200; //状态码
	int cgi = 0; //CGI的存在是为了处理后续的数据。url中的数据
	char *query_string; //指向参数字符串。
	get_line(sock, line, sizeof(line)); // line->\r,\r\n,\n -->\n
	//假如请求为 GET /a/a/a?name=zhangsan HTTP/1.1 下面将这个字符串解析
	while(i < sizeof(method)-1 && j < sizeof(line) && !isspace(line[j]) )
	{
		method[i] = line[j];
		i++, j++;
	}
	method[i] = 0;//获取到方法
	if(strcasecmp(method, "GET") == 0){
		
	}else if(strcasecmp(method, "POST") == 0)
	{
		cgi = 1; //在HTTP当中如果请求方法是POST，那么必须要以CGI方式运行，因为我们认定POST方法是传参的。
				//当然GET方法也可能以CGI方式运行
	}else{//如果不是GET也不是POST那么  将其头部清空，然后设置错误码，最后返回
		clear_header(sock);//清空头部
		status_code = 404;//设置错误码
		goto end;
	}
	//走到这里，要么是GET方法，要么是POST方法
	//获取url
	i = 0;
	//清除空格，让j指向url开始部分
	while(j < sizeof(line) && isspace(line[j]) )
	{
		j++;
	}
	while(i < sizeof(url)-1 && j < sizeof(line) && !isspace(line[j]) )
	{
		url[i] = line[j];
		j++, i++;
	}
	url[i] = 0;
#ifdef DEBUG
	printf("line: %s\n", line);
	printf("method: %s, url=: %s\n", method, url);
#endif

  //处理默认首页， 
  //mehod GET/POST,  url, GET(query_string)/POST, cgi
  sprintf(path, "wwwroot%s", url);//两种情况 wwwroot/,  wwwroot/a/b/c.html
  //如果字符串最后一个是斜杠，
  if(path[strlen(path)-1] == '/'){
  	strcat(path, HOME_PAGE);
  }
  
  struct stat st;
  if(stat(path, &st) < 0){
  	clear_header(sock);//清空头部
  	status_code = 404;//文件不存在，设置错误码
  	goto end;
  }else
  {
  	if(S_ISDIR(st.st_mode)){//是是不是目录
  		strcat(path, HOME_PAGE);
  	//可执行文件
  	}else if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)){
  		cgi = 1;
  	}else{
  		//是普通文件
  	}
  }

  	status_code = echo_wwww(sock, path, st.st_size);


end:
	if(status_code != 200)
	{
		echo_error(sock, status_code);
	}
	close(sock);
}

int startup(int port)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0)
	{
		perror("socket");
		exit(2);
	}
	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = htonl(INADDR_ANY);
	local.sin_port = htons(port);
	if(bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0)
	{
		perror("bind");
		exit(3);
	}
	
	if(listen(sock, 5) < 0)
	{
		perror("listen");
		exit(4);
	}
	return sock;
}

// ./httpd  80
int main(int argc, char *argv[])
{
	int listen_sock = startup(atoi(argv[1]));
	printf("%d\n", listen_sock);
	if(argc != 2)
	{
		usage(argv[0]);
		return 1;
	}
	//将服务器端口蛇尾可复用，防止关闭服务器后TIME_WAIT对服务器造成的影响
	int opt = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	for(;;){
		struct sockaddr_in client;
		socklen_t len = sizeof(client);
		int sock = accept(listen_sock, (struct sockaddr*)&client, &len);
		printf("%d\n", sock);
		if(sock < 0){
			perror("accept");
			continue;
		}
		printf("get a new client!\n");

		pthread_t tid;
		pthread_create(&tid, NULL, header_request, (void *)sock);
		pthread_detach(tid);
	}
}
