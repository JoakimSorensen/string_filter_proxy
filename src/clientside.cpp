#include "clientside.h"

ClientSide::ClientSide() {
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
}


void ClientSide::setUpConnection(int portNumber, std::string url) {
    if (url.empty()) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    std::string portNumberString = std::to_string(portNumber);

    if ((rv = getaddrinfo(url.c_str(), portNumberString.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        std::cout << "Error in client getaddrinfo..." << std::endl;
        goto addrerror;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    inet_ntop(p->ai_family, get_in_addr_client((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure
addrerror:
    std::cout << "client successfully connected" << std:: endl;
}

void ClientSide::closeSocket() {
    close(sockfd);
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr_client(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void ClientSide::freeServInfo(struct addrinfo *servinfo) {
    freeaddrinfo(servinfo); // free the linked-list
}

// sendall data to connected server
int ClientSide::sendall(char *buf, int *len) {
    int s = sockfd;
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) { break; }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}


// receive data from connected server
int ClientSide::receiveData(void* outbuf) {
    numbytes = recv(sockfd, outbuf, MAXDATASIZE-1, 0);
    if ((numbytes) == -1) {
        perror("recv");
        exit(1);
    }
    return numbytes;
}


// receive a stream of data from connected server
int ClientSide::receiveDataStream(void* outbuf) {
    numbytes = 1;
    int totalBytes = 0;
    while(numbytes != 0) {
        numbytes = recv(sockfd, outbuf, MAXDATASIZE - 1, 0);

        totalBytes += numbytes;
        if ((numbytes) == -1) {
            perror("recv");
            exit(1);
        }
    }
    return totalBytes;
}
