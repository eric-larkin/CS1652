#include "minet_socket.h"
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <string> /* added by eric */
#include <vector> /* added by eric */
#include <iostream>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int sock);

int main(int argc, char * argv[]) {
    int server_port = -1;
    int rc          =  0;
    /* eric */
    struct sockaddr_in server;
    fd_set readList;
    int socket = -1;
    int maxFD = -1;
    int accept = -1;

    /* parse command line args */
    if (argc != 3) {
	fprintf(stderr, "usage: http_server1 k|u port\n");
	exit(-1);
    }

    server_port = atoi(argv[2]);

    if (server_port < 1500) {
	fprintf(stderr, "INVALID PORT NUMBER: %d; can't be < 1500\n", server_port);
	exit(-1);
    }
    
    /* initialize and make socket */
    if(toupper(*(argv[1])) == 'K') {
	minet_init(MINET_KERNEL);
    }
    else if(toupper(*(argv[1])) == 'U') {
	minet_init(MINET_USER);
    }
    else {
	fprintf(stderr, "first argument must be k or u\n");
	exit(-1);
    }

    socket = minet_socket(SOCK_STREAM);
    if(!socket) {
	fprintf(stderr, "Error in minetsocket");
	minet_deinit();
        exit(-1);
    }

    /* set server address*/
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    server.sin_addr.s_addr = 0; //INADDR_ANY; /* equates to zero, but needs the addr_in type */

    /* bind listening socket */
    if(minet_bind(socket, (struct sockaddr_in *)&server) < 0) {
	fprintf(stderr, "error in minet_bind");
	minet_deinit();
	exit(-1);
    }
    /* start listening */
    if(minet_listen(socket, 50) < 0) {
	fprintf(stderr, "error in minet_listen");
	minet_deinit();
	exit(-1);
    }

    /* connection handling loop: wait to accept connection */
    maxFD = socket + 1;

    FD_ZERO(&readList);
    FD_SET(socket, &readList);

    fd_set con;
    FD_ZERO(&con);

    while (1) {
	
	/* create read list */ /* eric note - don't need to create a separate list, just use one */
	con = readList;

	/* do a select */
//	if(minet_select(maxFD, &readList, 0, 0, 0) > 0) {	
	if(minet_select(maxFD, &con, NULL, NULL, NULL) > 0) {
		/* process sockets that are ready */
		for(int i = 0; i < maxFD; i++) {
			/* need to check if the fd is in our list (i.e., not stderr, stdout, stdin) */
//			if(FD_ISSET(i, &readList)) {
			if(FD_ISSET(i, &con)) {
				fprintf(stdout, "i = %d\n", i);
				if(i == socket) {
					fprintf(stdout, "i = socket\n");
					/* for the accept socket, add accepted connection to connections */
					accept = minet_accept(socket, 0);
					if(accept > 0) {
						fprintf(stdout, "accept > 0: %d\n", accept);
						/* set read list */
						FD_SET(accept, &readList);
	
						/* its possible the max file descriptor for the select needs to be updated */
						if(accept >= maxFD) {
							maxFD = accept + 1;
						}
					}
				}
				else {
//					fprintf(stdout, "going to handle\n");
					rc = handle_connection(i);
					if(rc < 0) {
						fprintf(stderr, "error handling connection\n");
					}
					fprintf(stdout, "cleared: %d from readlist\n", i);	
					/* updated read list */
					FD_CLR(i, &readList);
	
					close(i);
				}
			}
			else {
				fprintf(stderr, "not in read list: %d", i);
			}
		}
	}
   } 
}

int handle_connection(int sock) {
    bool ok = false;

    const char * ok_response_f = "HTTP/1.0 200 OK\r\n"	\
	"Content-type: text/plain\r\n"			\
	"Content-length: %d \r\n\r\n";
    
    const char * notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"	\
	"Content-type: text/html\r\n\r\n"			\
	"<html><body bgColor=black text=white>\n"		\
	"<h2>404 FILE NOT FOUND</h2>\n"				\
	"</body></html>\n";
	FILE *fp;
	long fileSize;
	char *ok_return;
	int headLen;
	char *tokens[5];
	int i = 0;
	char buf[BUFSIZE];
	
	if(sock < 1) {
		fprintf(stdout, "sock error");
		return -1;
	}
    
    /* first read loop -- get request and headers*/
	std::string response ("");
	int bytesRet = -1;
	while(true) {
		memset(buf, 0, BUFSIZE - 1);
		
		bytesRet = minet_read(sock, buf, BUFSIZE - 1);
		if(bytesRet == BUFSIZE - 1) {
			response += std::string(buf);
		}
		else {
			response += std::string(buf);
			break;
		}
	}

    /* parse request to get file name */
    /* Assumption: this is a GET request and filename contains no spaces*/
	char *responseChar = &response[0u];
//	fprintf(stdout, "responsechar: %s\n", responseChar);

	tokens[0] = strtok(responseChar, " ");
	while(tokens[i] != NULL) {
		tokens[++i] = strtok(NULL, " ");
	}
	
    /* try opening the file */
	fp = fopen(tokens[1], "rb");
	
	if(fp != NULL){
		ok = true;
		fseek(fp, 0, SEEK_END);
	        fileSize = ftell(fp);
		rewind(fp);
		
	}else{
		fprintf(stderr, "CANNOT OPEN FILE\n");
		return -1;
	}
	
    /* send response */
    if (ok) {
	/* send headers */
	ok_return = (char *)malloc(sizeof(char) * strlen(ok_response_f));
	headLen = sprintf(ok_return, ok_response_f, fileSize);
	
	if(minet_write(sock, ok_return, headLen) != headLen){
		fprintf(stderr, "UNEXPECTED NUMBER OF BYTES WRITTEN TO BUFFER\n");
		exit(1);
	}
	
	/* send file */
	while(!feof(fp)) {
		fread(buf, 1, 1, fp);
		minet_write(sock, buf, 1);
	}
	
    } else {
	// send error response
	if(minet_write(sock, (char *)notok_response, sizeof(notok_response)) != sizeof(notok_response)) {
            fprintf(stderr, "WROTE UNEXPECTED NUMBER OF BYTES TO SOCKET\n");
            exit(1);
	} 
    }
    
    /* close socket and free space */
	minet_close(sock);
	//free(buf);    
	fclose(fp); 
 
    if (ok) {
	return 0;
    } else {
	return -1;
    }
}
