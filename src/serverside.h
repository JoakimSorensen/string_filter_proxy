#ifndef SERVERSIDE_H
#define SERVERSIDE_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include "clientside.h"
#include <vector>
#include <algorithm>

#define NUMBER_OF_FORBIDDEN_WORDS 6
#define HOST_OFF_SET 7

class ServerSide {
public:
    ServerSide();
    ServerSide(int portNumber);
    void freeServinfo(struct addrinfo *servinfo);
    void startConnectionWaiting();

private:
    int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
     struct addrinfo hints, *servinfo, *p;
     struct sockaddr_storage their_addr; // connector's address information
     socklen_t sin_size;
     struct sigaction sa;
     int yes=1;
     char s[INET6_ADDRSTRLEN];
     int rv;
     char buf[MAXDATASIZE];
     ClientSide clientSide; // client side to talk to server
     std::string userAgent; //  the currently used user agent

     std::string getUrl(char *buf);
     std::string getHost(char *buf);
     void convertToCloseConnection(char* buf);
     void receiveData();
     int sendall(int s, char *buf, int *len);
     void receiveAndSendData(char *buf);
     bool isContentTypeTxt(char *buf);
     bool filterTxt(std::string txt, bool isURL);
     void adaptUserAgent(char *buf);
     void extractUserAgent(char *buf);
};

#endif // SERVERSIDE_H
