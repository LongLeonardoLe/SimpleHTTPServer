# SimpleHTTPServer
Simple HTTP Server using `epoll()`, which is in C library, able to handle up to 60000 concurrent incoming connections.

Use *Poco* library to handle multithreading and mutex lock so that the program can be compiled with standard C++.
