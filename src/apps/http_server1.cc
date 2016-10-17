#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100

int handle_connection(int sock);

int main(int argc, char * argv[]) {
    int server_port = -1;
    int rc          =  0;
    int sock        = -1;
	minet_socket_types mode;

    /* parse command line args */
    if (argc != 3) {
	fprintf(stderr, "usage: http_server1 k|u port\n");
	exit(-1);
    }
	
	if(strcmp(argv[1], "k") == 0){
		mode = MINET_KERNEL;
	}
	else if(strcmp(argv[1], "u") == 0){
		mode = MINET_USER;
	}
	else{
		fprintf(stderr, "Must express usage mode: k|u");
		exit(-1);
	}

    server_port = atoi(argv[2]);

    if (server_port < 1500) {
	fprintf(stderr, "INVALID PORT NUMBER: %d; can't be < 1500\n", server_port);
	exit(-1);
    }

    /* initialize and make socket */
	if(minet_init(mode) < 0){
		fprintf(stderr, "INITIALIZATION OF MINET FAIL");
		exit(-1);
	}
	else if((sock = minet_socket(SOCK_STREAM < 0)) < 0){
		fprintf(stderr, "CANNOT CREATE SOCKET");
		exit(-1);
	}

    /* set server address*/
	struct sockaddr_in saddr;
	memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(server_port);

    /* bind listening socket */
	if(minet_bind(sock, (struct sockaddr_in*)&saddr) < 0) {
        fprintf(stderr, "SOCKET CANNOT BIND TO ADDRESS.");
        exit(-1);
	}
    /* start listening */
	if(minet_listen(sock, 50) < 0) {
        fprintf(stderr, "FAILED TO LISTEN ON SOCKET\n");
        exit(-1);
	}
    /* connection handling loop: wait to accept connection */

    while (1) {
	/* handle connections */
	rc = handle_connection(minet_accept(sock, (struct sockaddr_in*)&saddr));
    }
	
	minet_close(sock);
	minet_deinit();
}

int handle_connection(int sock) {
    bool ok = false;

    const char * ok_response_f = "HTTP/1.0 200 OK\r\n"	\
	"Content-type: text/plain\r\n"			\
	"Content-length: %d \r\n\r\n";
 
    const char * notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"	\
	"Content-type: text/html\r\n\r\n"			\
	"<html><body bgColor=black text=white>\n"		\
	"<h2>404 FILE NOT FOUND</h2>\n"
	"</body></html>\n";
	
	char *useFile;
	FILE *fp;
	long fileSize;
	
	char *ok_return;
	int headLen;

	char buf[BUFSIZE];
	
	if(sock < 1){
		return -1;
	}
    
    /* first read loop -- get request and headers*/
	char recvbuf[BUFSIZE];
	char totalrecv[BUFSIZE*1024];
	int filesize;
	int received = minet_read(sock, recvbuf, BUFSIZE);
	int recvpos = 0;
	do {
		if(received > 0) {
			memcpy(totalrecv+recvpos, recvbuf, received);
			recvpos += received;
			// last block already received
			if(received < BUFSIZE)
				break;
		}
		received = minet_read(sock, recvbuf, BUFSIZE);
	} while(received > 0 && (recvpos < (BUFSIZE*1024)));

	// display what was read to the screen
	printf("%s\n", totalrecv);
    
    /* parse request to get file name */
    /* Assumption: this is a GET request and filename contains no spaces*/
	if(strcmp(strtok(recvbuf," "), "GET") != 0){
			fprintf(stderr, "EXPECTED GET REQUEST. PLEASE RESEND\nIN FORm OF GET");
			exit(1);
	}
	useFile = strtok(NULL," ");
	printf("%s\n", useFile);
	
    /* try opening the file */
	fp = fopen(useFile, "r");
	
	if(fp != NULL){
		ok = true;
		fseek(fp, 0, SEEK_END);
        filesize = ftell(fp);
		rewind(fp);
		
	}else{
		fprintf(stderr, "CANNOT OPEN FILE");
		exit(1);
	}
	
    /* send response */
    if (ok) {
	/* send headers */
	headLen = sprintf(ok_return, ok_response_f, fileSize);
	
	if(minet_write(sock, ok_return, headLen) != headLen){
		fprintf(stderr, "UNEXPECTED NUMBER OF BYTES WRITTEN TO BUFFER");
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
	free(buf);
  
    if (ok) {
	return 0;
    } else {
	return -1;
    }
}
