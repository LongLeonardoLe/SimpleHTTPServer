#include <stdio.h> 
#include <string.h>   //strlen 
#include <stdlib.h> 
#include <errno.h> 
#include <unistd.h>   //close 
#include <arpa/inet.h>    //close 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/ioctl.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 
#include <string>
#include <queue>
#include <ctime>
#include <Poco/DateTime.h>
#include <iostream>
#include <asm-generic/ioctls.h>

#include "dispatcher.h"

#define PORT 9000

using namespace std;

void construct_message(string& message) {
    message = "HTTP/1.1 200 OK\r\n"
            "Server: Local\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            ;
    
    Poco::DateTime now;
    string time = "Date: ";
    time += to_string(now.day()) + " " + to_string(now.month()) + " " + to_string(now.year()) + " ";
    time += to_string(now.hour()) + ":" + to_string(now.minute()) + ":" + to_string(now.second()) + " GMT";
    
    string body = "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "<title>Test Header</title>\n"
            "<style>body{font-family: Verdana, Georgia}</style>\n"
            "</head>\n"
            "<body>\n"
            "<h1>Testing</h1>\n";
    body += "<p>" + time + "</p>\n";
    body += "<p>Extra lines to make the message is super duper long.</p>\n"
            "<p>Okay, it's not seriously long but you get the idea, so move on then.</p>\n"
            "</body>\n"
            "</html>"
            ;
    
    string content_length = "Content-Length: ";
    content_length += to_string(body.length()) + "\r\n";
    
    message += time + "\r\n";
    message += content_length + "Accept-Ranges: bytes\r\n" + "Connection: close\r\n";
    message += "\n" + body;
}

int main(int argc, char *argv[]) {
    int opt = 1;
    int backlog = 4000;
    int master_socket, addrlen;
    struct sockaddr_in address;

    // Create a master socket 
    if ((master_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP)) == 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    // Set master socket to allow multiple connections
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof (opt)) < 0) {
        perror("setsockopt()");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEPORT, (char *) &opt, sizeof (opt)) < 0) {
        perror("setsockopt()");
        exit(EXIT_FAILURE);
    }
    
    // Type of socket created 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket to address
    if (bind(master_socket, (struct sockaddr *) &address, sizeof (address)) < 0) {
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }
    printf("Server starts on port %d \n", PORT);
    
    string message;
    construct_message(message);
    const char* to_send = message.data();

    // Put the socket into listening state
    if (listen(master_socket, backlog) < 0) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    // Accept the incoming connection 
    addrlen = sizeof (address);
    puts("Waiting for connections ...\n");
    
    dispatcher _dispatch(master_socket, to_send);
    //_dispatch.start();
    _dispatch.start_with_epoll();

    return 0;
} 