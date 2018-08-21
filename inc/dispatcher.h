/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   dispatcher.h
 * Author: LongLNT
 *
 * Created on August 14, 2018, 3:17 PM
 */

#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdio.h> 
#include <string.h>   //strlen 
#include <stdlib.h> 
#include <errno.h> 
#include <unistd.h>   //close 
#include <netinet/in.h> 
#include <arpa/inet.h>    //close 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros 
#include <sys/epoll.h>
#include <Poco/ThreadPool.h>
#include <Poco/Runnable.h>
#include <Poco/RunnableAdapter.h>
#include <queue>
#include <thread>
#include <mutex>
#include <iostream>

#define MAX_QUEUED 128;
#define MAX_THREADS 100;

using namespace std;

class dispatcher : public Poco::Runnable {
public:    
    dispatcher(int master_socket, const char* msg);
    
    virtual ~dispatcher();
    
    // Put a sockfd to the queue
    void enqueue(int sockfd);
    
    // Pop the front sockfd from the queue
    int dequeue();
    
    // Continuously dequeue and send response on sockfd
    void run();
    
    // Start the dispatcher, continuously accept connections as long as
    // the queue is available
    void start();
    
    void start_with_epoll();
    
    void run_with_epoll();
    
    int make_socket_nonblocking(int sockfd);
    
    // Set the maximum number of sockfds to be queued
    void set_max_queued(int max_queued);
    
    // Set the maximum number of concurent threads
    void set_max_threads(int max_threads);
    
    const int get_max_queued() const;
    
    const int get_max_threads() const;
    
private:
    dispatcher();
    
    dispatcher(const dispatcher& orig);
    
    char* _default_msg;
    deque<int> _fd_queue;
    //mutex _mutex;
    Poco::FastMutex _mutex;
    Poco::ThreadPool _thread_pool;
    Poco::RunnableAdapter<dispatcher> _runnable;
    
    int _current_threads;
    int _master_socket;
    int _refused_conns;
    int _max_threads;
    int _max_queued;
    int _close_count;
    struct sockaddr_in _address;
    int _addrlen;
    
    int _epollfd;
    deque<int> _efd_queue;
    deque<epoll_event> _ev_queue;
};

inline void dispatcher::set_max_queued(int max_queued) {
    _max_queued = max_queued;
}

inline void dispatcher::set_max_threads(int max_threads) {
    _max_threads = max_threads;
}

inline const int dispatcher::get_max_queued() const {
    return _max_queued;
}

inline const int dispatcher::get_max_threads() const {
    return _max_threads;
}

#endif /* DISPATCHER_H */

