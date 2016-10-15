#include "minet_socket.h"
#include <stdlib.h>
#include <ctype.h>
#include <string> /* Added by Eric */
#define BUFSIZE 1024

int main(int argc, char * argv[]) {

    char * server_name = NULL;
    int server_port    = -1;
    char * server_path = NULL;
    char * req         = NULL;
    bool ok            = false;

    /* eric */
    int socket = -1;
    struct hostent *host = NULL;
    struct sockaddr_in server;
    fd_set set;
    char buffer[BUFSIZE];
    int bytesReturned = -1;
    std::string getResponse;
    int headerEndPosition = -1;
    
    /*parse args */
    if (argc != 5) {
	fprintf(stderr, "usage: http_client k|u server port path\n");
	exit(-1);
    }

    server_name = argv[2];
    server_port = atoi(argv[3]);
    server_path = argv[4];

    req = (char *)malloc(strlen("GET  HTTP/1.0\r\n\r\n")
			 + strlen(server_path) + 1);  

    /* initialize */
    if (toupper(*(argv[1])) == 'K') { 
	minet_init(MINET_KERNEL);
    } else if (toupper(*(argv[1])) == 'U') { 
	minet_init(MINET_USER);
    } else {
	fprintf(stderr, "First argument must be k or u\n");
	exit(-1);
    }

    /* make socket */
    socket = minet_socket(SOCK_STREAM);
    if(!socket) {
        fprintf(stderr, "Error in gethostbyname");
        free(req);
        minet_deinit();
        exit(-1);
    }
    
    /* get host IP address  */
    /* Hint: use gethostbyname() */
    host = gethostbyname(server_name);
    if(!host) {
        fprintf(stderr, "Error in gethostbyname");
        free(req);
        minet_deinit();
        exit(-1);
    }
    
    /* set address */
    //memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    server.sin_addr = *(struct in_addr *) host->h_addr;
//    server.sin_addr.s_addr = inet_addr(host->h_name);
    
    /* connect to the server socket */
    if(minet_connect(socket, (struct sockaddr_in *) &server) < 0) {   
//if(minet_connect(socket, (struct sockaddr_in *) &server) < 0) {
	fprintf(stderr, "Minet error: %d\n", minet_error());
        fprintf(stderr, "Error in minet_connect\n");
        free(req);
        minet_deinit();
        exit(-1);
    }
    
    /* send request message */
    sprintf(req, "GET %s HTTP/1.0\r\n\r\n", server_path);
    if(minet_write(socket, req, (sizeof(char) * strlen(req))) < 0) {
        fprintf(stderr, "Error in minet_write");
        free(req);
        minet_deinit();
        exit(-1);
    }
    
    /* wait till socket can be read. */
    /* Hint: use select(), and ignore timeout for now. */
    /* clear set */
    FD_ZERO(&set);
    
    /* set socket for listening */
    FD_SET(socket, &set);
    
    /* wait until it can be read
     Paramaters: 
        1) max socket to listen to (our socket + 1)
        2) set is put in 'read' paramater since that's what we're waiting for 
        3/4/5) not writing any descriptors or listening for errors or using a timeout
     */
    if(minet_select(socket + 1, &set, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "Error in minet_select");
        free(req);
        minet_deinit();
        exit(-1);
    }    

    /* first read loop -- read headers */
    bytesReturned = minet_read(socket, buffer, BUFSIZE - 1);
    if(bytesReturned <= 0) {
        fprintf(stderr, "Error in minet_read");
        free(req);
        minet_deinit();
        exit(-1);
    }
    
    //fprintf(stdout, buffer);    
    
    
    /* examine return code */   
    getResponse = std::string(buffer);
    
    /* Check return code */
    if(getResponse.substr(9,12) == "200") {
    	ok = true;
    }
    else {
	ok = false;
    }
    //Skip "HTTP/1.0"
    //remove the '\0'

    // Normal reply has return code 200

    /* print first part of response: header, error code, etc. */
    headerEndPosition = getResponse.find("\r\n\r\n", 0);
    /* working */fprintf(stdout, getResponse.substr(0, headerEndPosition).c_str());

    /* second read loop -- print out the rest of the response: real web content */
    getResponse = getResponse.substr(headerEndPosition, (getResponse.length() - headerEndPosition)); /* reset */
//    bytesReturned = minet_read(socket, buffer, BUFSIZE - 1);

    /*if(bytesReturned <= 0) {
	fprintf(stderr, "Error in minet_read 2");
	free(req);
	minet_deinit();
	exit(-1);
    }*/

    /* assuming header information was returned, this loop should be entered, as bytesReturned will still be greater than zero from original read */
    while(true) {
	memset(buffer, 0, BUFSIZE-1);
	if (minet_select(socket + 1, &set, NULL, NULL, NULL) < 0) {
		fprintf(stderr, "Error in minet_select LOOP");
	        free(req);
	        minet_deinit();
	        exit(-1);
	}

        bytesReturned = minet_read(socket, buffer, BUFSIZE - 1);
	if(bytesReturned > 0) {
	    	getResponse += std::string(buffer);
	}
	else {
		break;
	}
    }
//    getResponse += std::string(buffer);
    fprintf(stdout, getResponse.c_str());

    /*close socket and deinitialize */
    if(minet_close(socket) < 0) {
	fprintf(stderr, "Error in minet_close");
    }

    free(req);

    if (ok) {
	return 0;
    } else {
	return -1;
    }
}
