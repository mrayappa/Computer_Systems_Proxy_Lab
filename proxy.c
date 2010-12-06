/*
 * Proxy Lab
 * 15-213
 * 
 */

#define _GNU_SOURCE

#include "csapp.h"

typedef struct data
{
	char* hostname;
	char* pagecontent;
	time_t timestamp;
	void* next;
} data;


/* Default port will be port 80 */
#define DEFAULT_PORT 80

/* Length of the string "http://" */
#define HTTP_LEN 7

/* Define a kilobyte of data as 1024 bytes */
#define KILOBYTE 1024

/* The max cache size is 1 MB */
#define MAX_CACHE_SIZE 1048576

/* The max size of a piece of data in the cache is 100 KB */
#define MAX_OBJECT_SIZE 102400

/* Begin function headers */
void displayError(char* msg, int fd);

void initCache();
int getFromCache(char* buffer, char* url);
int addtoCache(char* buffer, int wsize, char* url);
int remLastFromCache();
void freeAll(data* p);

void handleRequest(int fd);
void *requestHelper(void* arg);
int parsePort(char* url);
char* parseURI(char* url);
char* parseHost(char* url);
/* End function headers */

/* Pointer to the beginning of the linked list cache */
data* LLCache;
int currentSize;

void initCache()
{
	LLCache = NULL;
	currentSize = 0;
}

int getFromCache(char* buf, char* url)
{
	data* pointer = LLCache;
 
	/* Search through Cache and find url */
	while (pointer != NULL){
		if (!strcmp(pointer->hostname, url)){
			memmove(buf, pointer->pagecontent, MAXLINE);
            pointer->timestamp = time();
			printf("\nfound something in cache.\n");
			/* found, return 1 */
			return 1;
		}		
		pointer = pointer->next;
	}

	printf("\nnothing in cache.\n");	
	/* If not found, return 0 */
	return 0;

}

int addtoCache(char* buffer, int wsize, char* url)
{
	data* pointer = LLCache;
	char* page = NULL;
	char* hostname = NULL;

	// see if current cache size + new obj >= 1MB
	while (currentSize + wsize >= MAX_CACHE_SIZE){
        printf("\nmax cache size reached, removing last used object\n");
		remLastFromCache();
	}

	
	// add buffer to cache, increase cache size
	pointer = malloc(sizeof(data));

	pointer->hostname = malloc(strlen(url)+1);
	memmove(pointer->hostname, url, strlen(url)+1);
		
	pointer->pagecontent = malloc(wsize);
	memmove(pointer->pagecontent, buffer, wsize);

	pointer->timestamp = time();
	pointer->next = LLCache;

	LLCache = pointer;

	currentSize += wsize;

    return 0;
}

int remLastFromCache()
{
    int earliest = INT_MAX;

    data* prevPointer = LLCache;
    data* currPointer;

    while (prevPointer != NULL){
        if (prevPointer->timestamp < earliest)
            earliest = prevPointer->timestamp;

        prevPointer = prevPoiner->next;
    }

    prevPointer = NULL;
    currPointer = LLCache;
    while (currPointer != NULL){
        if (currPointer->timestamp == earliest){
            if (prevPointer == NULL){
                LLCache = currPointer->next;
            } else{
                prevPointer->next = currPointer->next;
            }
            currentSize -= (strlen(currPointer->pagecontent) + 1);
            freeAll(currentPointer);
            break;
        }
        prevPointer = currPointer;
        currPointer = currPointer->next;
    }

    return 0;
}

void freeAll(data* p){
    free(p->hostname);
    free(p->pagecontent);
    free(p);
}

int main (int argc, char *argv [])
{
	int listenfd;
	int* connfdptr;
	int port;
	int clientlen;
    struct sockaddr_in clientaddr;
	pthread_t tid;

	Signal(SIGPIPE, SIG_IGN);

	initCache();

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	/* Open a listening socket */
	port = atoi(argv[1]);
    listenfd = open_listenfd(port);

	/* Keep looping and listening for a connection request */
    while(1)
	{
    	clientlen = sizeof(clientaddr);

    	connfdptr = malloc(sizeof(int));

		if(connfdptr == NULL)
			exit(1);

		*connfdptr = accept(listenfd, (SA *)&clientaddr, &clientlen);

		if(*connfdptr == -1)
			exit(1);

		pthread_create(&tid, NULL, requestHelper, connfdptr);
			
	}
}

void handleRequest(int fd)
{
	char buffer[MAXLINE];
	char method[MAXLINE];
	char url[MAXLINE];
	char version[MAXLINE];
    
	char host_string[MAXLINE];
	char* request_string;

	char* host;
	char* uri;   
	char* helper;
	
	rio_t rio;
	rio_t rioserver;
	
	int port; 
	int clientfd;
   	int n;

	char toCache[MAX_OBJECT_SIZE];
	int endCacheIndex = 0;
	int exceedMaxObjSize = 0;

	rio_readinitb(&rio,fd);

	/* Flush all the char arrays to start */
	memset(buffer,'\0',MAXLINE);
	memset(method,'\0',MAXLINE);
	memset(url,'\0',MAXLINE);
	memset(version,'\0',MAXLINE);
	memset(host_string,'\0',MAXLINE);
	memset(toCache,'\0',MAX_OBJECT_SIZE);

	/* Read the GET request and extract the 3 parts */
	rio_readlineb(&rio, buffer, MAXLINE);
	sscanf(buffer, "%s %s %s", method, url, version);

	if(getFromCache(buffer,url) == 1)
	{
//		rio_writen(fd,buffer,___);
		return;
	}

	/* Parse the URL into a host, URI and port number */
	port = parsePort(url);
	host = parseHost(url);
	uri = parseURI(url);

	/* Create the string for a GET request */	
	request_string = strcat(method, " ");
	strcat(request_string,uri);
	strcat(request_string," ");
	strcat(request_string,"HTTP/1.0");
	strcat(request_string,"\r\n");

	/* Create the Host header */
	strcat(host_string,"Host: ");	
	strcat(host_string,host);
	strcat(host_string,"\r\n");

	/* Open a socket with the web server */
	clientfd = open_clientfd(host,port);

	/* Send the GET request and the Host header */
	rio_writen(clientfd, request_string, strlen(request_string));
	rio_writen(clientfd, host_string, strlen(host_string));

	/* Flush the buffer */
	memset(buffer,'\0',MAXLINE);
	
	/* Loop through the remaining headers and send them to the web server */

	while(strncmp(buffer,"\r\n",2) != 0)
	{
		memset(buffer,'\0',MAXLINE);
		rio_readlineb(&rio,buffer,MAXLINE);

		helper = strcat(buffer,"\r\n");
		

		/* Send "Connection: close" header  but don't send the Keep-Alive
		 * header.  Also, don't send the Host header since it's already been
		 * sent.
		 */
		if(strcasestr(helper,"Connection") != NULL)
		{
			helper = "Connection: close\r\n";
			rio_writen(clientfd,helper,strlen(helper));
		}
		else if(strcasestr(helper, "Proxy-Connection") != NULL)
		{
			helper = "Connection: close\r\n";
			rio_writen(clientfd,helper,strlen(helper));
		}
		else if(strcasestr(helper,"Keep-Alive") != NULL
										|| strcasestr(helper,"Host") != NULL)
		{
			/* Don't send these headers */
		}
		else
		{
			rio_writen(clientfd,helper,strlen(helper));	
		}
	}

	/* Terminate the request headers */
	rio_writen(clientfd,"\r\n",2);

	/* Flush the buffer */
	memset(buffer,'\0',MAXLINE);
	
	rio_readinitb(&rioserver,clientfd);

	/* Read in data from the server and pass it on to the browser */	
	while((n = rio_readnb(&rioserver,buffer,KILOBYTE)) != 0)
	{
		if(n == -1)
		{
			displayError("500 Internal Server Error",fd);
			return;
		}
		rio_writen(fd,buffer,n);

		if(!exceedMaxObjSize){
			
			if ((endCacheIndex + n) < MAX_OBJECT_SIZE){
				memcpy(toCache + endCacheIndex, buffer, n);
				endCacheIndex += n;
			} else
				exceedMaxObjSize = 1;

		}
		memset(buffer,'\0',MAXLINE);
	}

	if (!exceedMaxObjSize)
		addtoCache(toCache, endCacheIndex, url);	
	free(host);
}

void displayError(char* msg,int fd)
{
	char error[MAXLINE];
	char* errstring = strcat(error, "<html><h1><b>Error</b></h1><p><p><h2><b>");
	strcat(errstring,msg);
	strcat(errstring,"</b></h2><p><p><h3><b>Please try again later");
	strcat(errstring,"</b></h3></html>");
	rio_writen(fd, error, strlen(error));
	return;
}


void* requestHelper(void* arg)
{
	int connfd = *((int*)arg);
	pthread_detach(pthread_self());
	free(arg);
	handleRequest(connfd);
	close(connfd);
	return NULL;
}

int parsePort(char* url)
{
	int i;
	char* ptr = url;
	int length = strlen(url);
	int flag;

	if(strncasecmp(url,"http://",HTTP_LEN) == 0)
	{
		ptr = ptr+HTTP_LEN;
		length = length-HTTP_LEN;
	}

	for(i = 0; i<length; i++)
	{
		if(ptr[i] == '/')
		{
			flag = 1;
		}

		if(ptr[i] == ':' && flag == 0)
		{
			return atoi(ptr+i+1);
		}
	}
	return DEFAULT_PORT;
	
}

char* parseURI(char* url)
{
	int i;
	int length = strlen(url);
	char* ptr = url;

	if(strncasecmp(url,"http://",HTTP_LEN) == 0)
	{
		ptr = ptr+HTTP_LEN;
		length = length-HTTP_LEN;
	}
	
	for(i = 0; i<length;i++)
	{
		if(ptr[i] == '/')
			return ptr+i;
	}
	return "/";

}

char* parseHost(char* url)
{
	int i;
	int length = strlen(url);
	char* ptr = url;
	char* final;

	if(strncasecmp(url,"http://",HTTP_LEN) == 0)
	{
		ptr = ptr+HTTP_LEN;
		length = length-HTTP_LEN;
	}
	for(i = 0; i<length; i++)
	{
		if(ptr[i] == '/' || ptr[i] == ':')
			break;
	}
	final = malloc(i+1);
	strncpy(final,ptr,i);
	final[i] = '\0';
	return final;
}
