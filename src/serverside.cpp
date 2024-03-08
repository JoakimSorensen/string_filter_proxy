#include "serverside.h"

void sigchld_handler(int s);
void *get_in_addr_serv(struct sockaddr *sa);


ServerSide::ServerSide(int portNumber) {
    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    std::string const portNumberString = std::to_string(portNumber);

    if ((rv = getaddrinfo(NULL, portNumberString.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
        //return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    // initialise the client side
    clientSide = ClientSide();

}

void ServerSide::freeServinfo(struct addrinfo *servinfo) {
    freeaddrinfo(servinfo); // free the linked-list
}

void ServerSide::startConnectionWaiting() {
    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr_serv((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener

            // ServerSide receive from client
            receiveData();
            // Set connection to close
            convertToCloseConnection(buf);

            // Copy the buffer and number of bytes
            // to be able to resend the data
            char copyBuf[MAXDATASIZE];
            strcpy(copyBuf, buf);
            int copyNumbytes = numbytes;

            std::string url = getUrl(buf);
            std::string host = getHost(buf);

            // Check if the url is ok
            bool redirect = filterTxt(url, true);

            if(!redirect) {
                clientSide.setUpConnection(80, host);
                clientSide.sendall(buf, &numbytes);
            }

            // get header plus possible additional data
            char additionalHeaderBuf[MAXDATASIZE];
            int receivedHeaderBytes = clientSide.receiveData(additionalHeaderBuf);
            // check if it is some kind of text
            if(isContentTypeTxt(additionalHeaderBuf)) {
                // receive all data to filter
                int receivedBytes = clientSide.receiveDataStream(buf);
                std::string bufString(buf);
                std::string additionalHeaderBufString(additionalHeaderBuf);
                // filter and redirect if inappropiate
                if(filterTxt(bufString, false) || filterTxt(additionalHeaderBufString, false)) {
                    receivedBytes = clientSide.receiveDataStream(buf);
                    sendall(new_fd, buf, &receivedBytes);
                } else {
                    // otherwise proceed and reset the connection
                    clientSide.closeSocket();
                    clientSide.setUpConnection(80, host);
                    clientSide.sendall(copyBuf, &copyNumbytes);
                }

            } else {
                // need the previously gathered header and data if
                // connection was not reset
                sendall(new_fd, additionalHeaderBuf, &receivedHeaderBytes);
            }

            receiveAndSendData(buf);

            close(new_fd);
            clientSide.closeSocket();
            exit(0);
        }
        close(new_fd); // parent doesn't need this
    }
}


// receive data from the connected client
void ServerSide::receiveData() {
    numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0);

    if ((numbytes) == -1) {
        perror("recv");
        exit(1);
    }
    buf[numbytes] = '\0';
}


// send all data to be sent to a fd s
int ServerSide::sendall(int s, char *buf, int *len) {
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1) {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n==-1?-1:0; // return -1 on failure, 0 on success
}


// extract url from a get request from the client
std::string ServerSide::getUrl(char *buf) {
    std::string url;
    int ind = 11;
    while(buf[ind] != ' ') {
        url += buf[ind];
        ++ind;
    }
    if(url[url.size() - 1] == '/') {
        url = url.erase(url.size()-1);
    }

    std::cout << "our url: " << url << std::endl;
    return url;
}


// extract the host from the get request of the client
std::string ServerSide::getHost(char *buf) {
    std::string host;
    int ind = 0;
    while(buf[ind] != '\n') {
        ++ind;
    }
    ind += HOST_OFF_SET;
    while(buf[ind] != '\r') {
        host += buf[ind];
        ++ind;
    }
    std::cout << "our host: " << host << std::endl;
    return host;
}


// change the connection type form keep-alive to close for a
// given request
void ServerSide::convertToCloseConnection(char* buf) {
    std::string keepAliveString = "keep-alive";
    std::string stringBuf(buf);

    while(stringBuf.find(keepAliveString) != std::string::npos) {
        stringBuf.replace(stringBuf.find(keepAliveString),
                          keepAliveString.length(), "close");
    }
    strcpy(buf, stringBuf.c_str());
}


void sigchld_handler(int s) {
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr_serv(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


// sends the data which the client side receives to the client
void ServerSide::receiveAndSendData(char *buf) {
    int numberOfBytes = 1;
    while(numberOfBytes != 0) {
        numberOfBytes = clientSide.receiveData(buf);
        int sendSuccess = sendall(new_fd, buf, &numberOfBytes);
        if (sendSuccess == -1)
            perror("send");

    }
}


// extract the content type from a particular request
bool ServerSide::isContentTypeTxt(char *buf) {
    std::string contentTypeTxt = "Content-Type: text";
    std::string stringBuf(buf);
    bool res = (stringBuf.find(contentTypeTxt) != std::string::npos);
    return res;
}


// filter a string, as to not contain one of the forbidden words
bool ServerSide::filterTxt(std::string txt, bool isURL) {
    std::vector<std::string> forbiddenWords;
    std::string smallTxt = txt;
    const std::string fw[] = {"spongebob", "britney spears", "paris hilton", "norrkoping", "norrköping", "misadventures"};

    for(int i = 0; i < NUMBER_OF_FORBIDDEN_WORDS; ++i) {
        forbiddenWords.push_back(fw[i]);
    }

    std::transform(smallTxt.begin(), smallTxt.end(), smallTxt.begin(), ::tolower);

    // the http get for url redirect
    char urlRedirect[] = "GET http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error1.html HTTP/1.1\r\nHost: "
            "www.ida.liu.se\r\n User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.0.2)"
            "Gecko/20021120 Netscape/7.01\r\nAccept: text/xml,application/xml,application/xhtml+xml,"
            "text/html;q=0.9,text/plain;q=0.8,video/x-mng,image/png,image/jpeg,image/gif;q=0.2,text/css,"
            "*/*;q=0.1\r\nAccept-Language: en-us, en;q=0.50\r\nAccept-Encoding: gzip, deflate, "
            "compress;q=0.9\r\nAccept-Charset: ISO-8859-1, utf-8;q=0.66,"
            "*;q=0.66\r\nKeep-Alive: 300\r\nConnection: close\r\n\r\n";

    // the http get for web redirect
    char webRedirect[] = "GET http://www.ida.liu.se/~TDTS04/labs/2011/ass2/error2.html HTTP/1.1\r\nHost: "
            "www.ida.liu.se\r\n User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.0.2)"
            "Gecko/20021120 Netscape/7.01\r\nAccept: text/xml,application/xml,application/xhtml+xml,"
            "text/html;q=0.9,text/plain;q=0.8,video/x-mng,image/png,image/jpeg,image/gif;q=0.2,text/css,"
            "*/*;q=0.1\r\nAccept-Language: en-us, en;q=0.50\r\nAccept-Encoding: gzip, deflate,"
            "compress;q=0.9\r\nAccept-Charset: ISO-8859-1, utf-8;q=0.66,"
            "*;q=0.66\r\nKeep-Alive: 300\r\nConnection: close\r\n\r\n";

    adaptUserAgent(urlRedirect);
    adaptUserAgent(webRedirect);

    for(std::string word : forbiddenWords) {
        if (smallTxt.find(word) != std::string::npos) {
            if (isURL) {
                int len = strlen(urlRedirect);
                clientSide.setUpConnection(80, getHost(urlRedirect));
                clientSide.sendall(urlRedirect, &len);
                return true;
            } else {
                int len = strlen(webRedirect);
                clientSide.setUpConnection(80, getHost(webRedirect));
                clientSide.sendall(webRedirect, &len);
                return true;
            }
        }
    }
    return false;
}


// changes updates the user agent to currently used for a
// given request
void ServerSide::adaptUserAgent(char *buf) {
    std::string userAgentTxt = "User-Agent: ";
    std::string stringBuf(buf);
    std::string userAgentField = "";
    int startIdx;
    if (stringBuf.find(userAgentTxt) != std::string::npos) {
        startIdx = stringBuf.find(userAgentTxt) + userAgentTxt.size();
        while(buf[startIdx] != '\r') {
            userAgentField += buf[startIdx];
            ++startIdx;
        }
    }

    while(stringBuf.find(userAgentField) != std::string::npos) {
        stringBuf.replace(stringBuf.find(userAgentField),
                          userAgentField.length(), userAgent);
    }
    strcpy(buf, stringBuf.c_str());

}


// extract the user agent and set the private field
void ServerSide::extractUserAgent(char *buf) {
    std::string userAgentTxt = "User-Agent: ";
    std::string stringBuf(buf);
    std::string userAgentField = "";

    int startIdx;
    if (stringBuf.find(userAgentTxt) != std::string::npos) {
        startIdx = stringBuf.find(userAgentTxt) + userAgentTxt.size();
        while(buf[startIdx] != '\r') {
            userAgentField += buf[startIdx];
            ++startIdx;
        }
        userAgent = userAgentField;
    }
}
