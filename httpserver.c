#include <sys/socket.h> 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <zlib.h>

char ROOT_DIR[200] 	= "./www";
short int gzipFlag 	= 1;
int port		= 80;

int gzcompress(Bytef *data, uLong ndata, Bytef *zdata, uLong *nzdata)
{
	z_stream c_stream;
	int err = 0;
	if(data && ndata > 0)
	{
		c_stream.zalloc = (alloc_func)0;
		c_stream.zfree = (free_func)0;
        	c_stream.opaque = (voidpf)0;
        	if(deflateInit2(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) return -1;
        	c_stream.next_in = data;
        	c_stream.avail_in = ndata;
        	c_stream.next_out = zdata;
        	c_stream.avail_out = *nzdata;
        	while (c_stream.avail_in != 0 && c_stream.total_out < *nzdata) 
        	{
        		if(deflate(&c_stream, Z_NO_FLUSH) != Z_OK) return -1;
        	}
        	if(c_stream.avail_in != 0) return c_stream.avail_in;
        	while (1) {
            		if((err = deflate(&c_stream, Z_FINISH)) == Z_STREAM_END) break;
            		if(err != Z_OK) return -1;
        	}
        	if(deflateEnd(&c_stream) != Z_OK) return -1;
        	*nzdata = c_stream.total_out;
        	return 0;
    	}
    	return -1;
}


struct ResponseData
{
	char* path;
	char* buf;
	int len;
	struct ResponseData *next;
} *firstData;


struct ResponseData* GetResponseData(char *path)
{
	struct ResponseData * data = firstData;
	while(data->next != NULL)
	{
		if(strcmp(path, data->path) == 0)
			return data;
		else
			data = data->next;
	}
	return data;
}

void *HttpResponse(void *client)
{
	int client_fd = *(int*)client;
	char recvBuf[5000];
	recv(client_fd, recvBuf, sizeof(recvBuf), 0);
	char *token = strtok(recvBuf," ");
	if(token != NULL){
        		token = strtok(NULL, " ");
	}
	if(token == NULL) token = ".404";
	if(strcmp(token, "/") == 0) token = "/index.html";
	printf("request:\t%s\n", token);
	struct ResponseData* data = GetResponseData(token);
	printf("response:\t%s\n", data->path);
	send(client_fd, data->buf, data->len, 0);
	shutdown(client_fd, SHUT_RDWR);
	close(client_fd);
	pthread_exit((void *)0);
}

void *ListenClient(void *server)
{
	int server_fd = *(int*)server;
	int len = sizeof(struct sockaddr);
	struct sockaddr_in client_addr;
	pthread_t ntid;
	while(1)
	{
		int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &len);
		struct timeval timeout = {5, 0};
		setsockopt(client_fd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));
		setsockopt(client_fd,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout,sizeof(struct timeval));
		char buf[30];
		printf("connect:\t%s:%d\n",inet_ntop(AF_INET, &client_addr.sin_addr, buf, sizeof(buf)),(unsigned int)ntohs(client_addr.sin_port));
		pthread_create(&ntid, NULL, HttpResponse, &client_fd);
		pthread_detach(ntid);
	}	
}


char* gzipHtml(char* data, unsigned long *len)
{
	unsigned long zlen = *len;
	char *gzipHtmlData = (char *)malloc(zlen * sizeof(char));
	gzcompress(data, *len, gzipHtmlData, &zlen);
	free(data);
	*len = zlen;
	return gzipHtmlData;
}

void HtmlRead(char *path, struct ResponseData* data)
{
	FILE *fp;
	fp = fopen(path, "r");
	fseek(fp, 0, SEEK_END);
	unsigned long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *fdata = (char *)malloc(len * sizeof(char));
	fread(fdata, len, sizeof(char), fp);
	fclose(fp);
	if(gzipFlag) fdata = gzipHtml(fdata, &len);
	data->buf = (char *)malloc((len + 100) * sizeof(char));
	char* blogHtml = data->buf;
	char sendBuf[] = "HTTP/1.1 200 OK\r\nServer:BlogServer\r\nContent-Length:";
	strcpy(blogHtml, sendBuf);
	sprintf(blogHtml + sizeof(sendBuf) - 1, "%ld", len);
	if(gzipFlag) strcat(blogHtml, "\r\nContent-Encoding:gzip");
	strcat(blogHtml, "\r\n\r\n");
	int marklen = strlen(blogHtml);
	memcpy(blogHtml + marklen, fdata, len * sizeof(char));
	free(fdata);
	data->len = marklen + len;
	data->path = (char *)malloc(strlen(path) * sizeof(char));
	memcpy(data->path, path + strlen(ROOT_DIR), strlen(path) - strlen(ROOT_DIR) + 1);
	printf("url:\t%s\n", data->path);
	printf("size:\t%d\n", data->len);
	struct ResponseData *newdata = (struct ResponseData*)malloc(sizeof(struct ResponseData));
	newdata->next = NULL;
	data->next = newdata;
}

struct ResponseData * HtmlInit(char *path, struct ResponseData *data)
{
	DIR *pDir;
	struct dirent *ent;
	pDir = opendir(path);
	while((ent = readdir(pDir)) != NULL)
	{
		char child[512];
		memset(child, 0, sizeof(child));
		sprintf(child, "%s/%s", path, ent->d_name);
		if(ent->d_type & DT_DIR)
		{
			if(ent->d_name[0] == '.')
				continue;
			data = HtmlInit(child, data);
		}
		else
		{
			HtmlRead(child, data);
			data = data->next;
		}
	}
	return data;
}

void Create404Res()
{
	struct ResponseData * data = firstData;
	while(data->next != NULL)
		data = data->next;
	data->path = ".404";
	data->buf = "HTTP/1.1 200 NOT FOUND\r\nServer:BlogServer\r\nContent-Length:18\r\n\r\n<h1>NOT FOUND</h1>";
	data->len = strlen(data->buf);
}


void ShowAllData()
{
	struct ResponseData * data = firstData;
	while(data != NULL)
	{
		printf("%s\n", data->path);
		data = data->next;
	}
}


int main(int argc,char* argv[])
{
	if(argc >= 2) port = atoi(argv[1]);
	if(argc >= 3) memcpy(ROOT_DIR, argv[2], strlen(argv[2]));
	if(argc >= 4) {
		if(strcmp(argv[3], "nogzip") == 0) gzipFlag = 0;
		else if(strcmp(argv[3], "gzip") == 0) gzipFlag = 1;
	}
	if(argc >= 5) {
		printf("invalid argument!"); return 1;
	}
	int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	struct sockaddr_in server_addr;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	int val = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(int));
	if(!bind(server_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr))) {char buf[30];printf("bind:%s:%d\n",inet_ntop(AF_INET, &server_addr.sin_addr, buf, sizeof(buf)),(unsigned int)ntohs(server_addr.sin_port));}
	else return 1;
	if(!listen(server_fd, 100)) {char buf[30];printf("listen:%s:%d\n",inet_ntop(AF_INET, &server_addr.sin_addr, buf, sizeof(buf)),(unsigned int)ntohs(server_addr.sin_port));}
	else return 1;

	printf("load:\nroot path:%s\n", ROOT_DIR);
	firstData = (struct ResponseData*)malloc(sizeof(struct ResponseData));
	struct ResponseData *pData = firstData;
	HtmlInit(ROOT_DIR, pData);
	Create404Res();
	printf("load finish!\n");
	//ShowAllData();

	pthread_t ntid;
	int ret = pthread_create(&ntid, NULL, ListenClient, &server_fd);
	if(ret){ printf("thread create error!\nerror code = %d", ret); return -1; }

	while(1) sleep((unsigned)1000);

	close(server_fd);
	return 0;
}


