#include <iostream>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "serverside.h"
#include "clientside.h"

using namespace std;
#define MYPORT 3490
int main(int argc, char *argv[]) {
    string inputString;
    char input;
    int chosenPort = MYPORT;

chooseAgain:
    cout << "Choose port to run the proxy on, press enter to run on default: ";

    // get all chars and make a string
    while(input != '\n') {
        cin.get(input);
        inputString += input;
    }
    cout << endl;

    size_t sz;
    // convert string to int if port was entered
    if(inputString != "\n") chosenPort = stoi(inputString, &sz);

    cout << "chosenPort: " << chosenPort << endl;

    if(chosenPort <= 1023) {
        std::cout << "Port number must be fatter than yo' momma!" << std::endl;
        goto chooseAgain;
    }

    ServerSide serverSide = ServerSide(chosenPort);

    cout << "success"<< endl;

    // parent waiting for input
    if(fork()) {
        char input;
wait_again:
        cout << endl << "Enter Q to terminate server" << endl;
        cin >> input;
        if(input == 'Q') {
            kill(0, SIGTERM);
        }

        goto wait_again;

    } else {
        if (!fork()) {
            // child waiting for connections
            serverSide.startConnectionWaiting();
        }
    }

    return 0;
}
