#ifndef CLIENTSIDE_H
#define CLIENTSIDE_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <arpa/inet.h>

#define BACKLOG 10
#define MAXDATASIZE 200000 // max number of bytes we can get at once


void *get_in_addr_client(struct sockaddr *sa);
class ClientSide
{
public:
    ClientSide();
    ClientSide(int portNumber, std::string url);

    void setUpConnection(int portNumber, std::string url);

    void freeServInfo(struct addrinfo *servinfo);

    void closeSocket();

    int sendall(char *buf, int *len);

    int receiveData(void *outbuf);

    int receiveDataStream(void *outbuf);

private:
    int sockfd, numbytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char buf[MAXDATASIZE];
};

#endif // CLIENTSIDE_H
