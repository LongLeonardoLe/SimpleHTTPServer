/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   dispatcher.cpp
 * Author: LongLNT
 * 
 * Created on August 14, 2018, 3:17 PM
 */

#include <fcntl.h>

#include "dispatcher.h"

dispatcher::dispatcher(int master_socket, const char* msg) :
_close_count(0),
_current_threads(0),
_master_socket(master_socket),
_refused_conns(0),
_thread_pool(),
_runnable(*this, &dispatcher::run) {
    _max_queued = MAX_QUEUED;
    _max_threads = MAX_THREADS;

    _default_msg = new char[strlen(msg)];
    for (int i = 0; i < strlen(msg); ++i) {
        _default_msg[i] = msg[i];
    }

    _thread_pool.addCapacity(_max_threads - _thread_pool.capacity());

    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY;
    _address.sin_port = htons(9000);
    _addrlen = sizeof (_address);
}

dispatcher::~dispatcher() {
    close(_master_socket);
    _thread_pool.stopAll();
}

int dispatcher::dequeue() {
    //_mutex.lock();
    Poco::FastMutex::ScopedLock lock(_mutex);
    int ret;
    if (!_fd_queue.empty()) {
        ret = _fd_queue.front();
        _fd_queue.pop_front();
    } else {
        ret = -1;
    }
    //_mutex.unlock();
    return ret;
}

void dispatcher::enqueue(int sockfd) {
    // Push the sockfd to the end of the queue if available
    //_mutex.lock();
    //Poco::FastMutex::ScopedLock lock(_mutex);
    //if (_fd_queue.size() < _max_queued) {
    _fd_queue.push_back(sockfd);
    if (/*!_fd_queue.empty() &&*/ (_current_threads < _max_threads)) {
        try {
            //_thread_pool.start(_runnable);
            _thread_pool.start(*this);
        } catch (Poco::Exception ex) {
        }
        ++_current_threads;
    } else {
        ++_refused_conns;
    }
    //}
    //_mutex.unlock();
}

void dispatcher::start() {
    fd_set readfds;
    int conn_fd;
    //int num_ready;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    while (true) {
        FD_ZERO(&readfds);
        FD_SET(_master_socket, &readfds);
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0) {
            perror("select()");
            exit(EXIT_FAILURE);
        }

        _mutex.lock();
        if (_fd_queue.size() < _max_queued) {
            conn_fd = accept(_master_socket, (struct sockaddr *) &_address, (socklen_t*) & _addrlen);
            if (conn_fd < 0) {
                perror("accept()");
                exit(EXIT_FAILURE);
            } else {
                enqueue(conn_fd);
            }
        }
        _mutex.unlock();
    }
}

void dispatcher::run() {
    char buffer[2049];
    fd_set readfds;
    int num_ready, sockfd, val_read;

    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    for (;;) {
        sockfd = dequeue();
        if (sockfd >= 0) {
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);
            if ((num_ready = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout)) <= 0) {
                getpeername(sockfd, (struct sockaddr*) &_address, (socklen_t*) & _addrlen);
                close(sockfd);
                FD_CLR(sockfd, &readfds);
                //_mutex.lock();
                //printf("select = %d. Closing sockfd %d on port %hu.\n", num_ready, sockfd, ntohs(_address.sin_port));
                //_mutex.unlock();
                continue;
            }

            if ((val_read = read(sockfd, buffer, 2048)) == 0) {
                //getpeername(sockfd, (struct sockaddr*) &_address, (socklen_t*) & _addrlen);
                close(sockfd);
                FD_CLR(sockfd, &readfds);
                continue;
            }
            if (send(sockfd, _default_msg, strlen(_default_msg), 0) != strlen(_default_msg)) {
                perror("send()");
            }
            close(sockfd);
            FD_CLR(sockfd, &readfds);
        }

        _mutex.lock();
        if (_fd_queue.empty() && _current_threads > 0) {
            --_current_threads;
            _mutex.unlock();
            break;
        }
        _mutex.unlock();
    }
}

void dispatcher::start_with_epoll() {
    struct epoll_event ev;

    // Create the epoll instance
    _epollfd = epoll_create1(0);
    if (_epollfd < 0) {
        perror("epoll_create1()");
        exit(EXIT_FAILURE);
    }

    // Register the master_socket to the epoll instance
    // Pay attention to EPOLLONESHOT, remember to re-arm it after epoll_wait() is called
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = _master_socket;
    make_socket_nonblocking(_master_socket);
    if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, _master_socket, &ev) < 0) {
        perror("epoll_ctl - master_socket");
        exit(EXIT_FAILURE);
    }

    // Start all threads
    for (int i = 0; i < 1; ++i) {
        Poco::RunnableAdapter<dispatcher> runnable(*this, &dispatcher::run_with_epoll);
        _thread_pool.start(runnable);
    }
    _thread_pool.joinAll();
}

void dispatcher::run_with_epoll() {
    int conn_fd, _nfds;
    ssize_t read_ret;
    int max_events = 100;
    struct epoll_event ev, events[max_events];
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    ev.data.fd = _master_socket;
    char buffer[512];

    for (;;) {
        _nfds = epoll_wait(_epollfd, events, max_events, -1);
        if (_nfds < 0) {
            perror("epoll_wait()");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < _nfds; ++i) {
            // Check error and hung up issues
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                close(events[i].data.fd);
                continue;
            }
            // The case when stream socket peer closed connection
            /*else if (events[i].events & EPOLLRDHUP) {
                cout << "Peer closed" << endl;
                close(events[i].data.fd);
                continue;
            }*/
            // If master_socket in the list, accept incoming connection
            else if (events[i].data.fd == _master_socket) {
                // Accept incoming connections
                conn_fd = accept(_master_socket, (struct sockaddr *) &_address, (socklen_t *) & _addrlen);

                // Re-arm the master_socket since it is using EPOLLONESHOT
                ev.data.fd = _master_socket;
                epoll_ctl(_epollfd, EPOLL_CTL_MOD, _master_socket, &ev);

                // Check the sockfd of the just accepted connection
                if (conn_fd < 0) {
                    perror("accept()");
                    exit(EXIT_FAILURE);
                }
                // Make sockfd nonblocking
                if (make_socket_nonblocking(conn_fd) < 0) {
                    exit(EXIT_FAILURE);
                }

                ev.events = EPOLLIN;
                ev.data.fd = conn_fd;
                // Registered the sockfd to the epoll instance
                if (epoll_ctl(_epollfd, EPOLL_CTL_ADD, conn_fd, &ev) < 0) {
                    perror("epoll_ctl - conn_fd");
                }
            }
            // Do stuff with fds
            else {
                // Read attempt
                read_ret = read(events[i].data.fd, buffer, 511);

                // If errno is EAGAIN, which means all data has been read, close the sockfd
                if (read_ret < 0) {
                    if (errno != EAGAIN) {
                        perror("read()");
                        close(events[i].data.fd);
                    }
                    continue;
                }// End of file, close the sockfd
                else if (read_ret == 0) {
                    close(events[i].data.fd);
                    continue;
                }
                // Send attempt
                if (send(events[i].data.fd, _default_msg, strlen(_default_msg), 0) != strlen(_default_msg)) {
                    perror("send()");
                }
                close(events[i].data.fd);
            }
        }
    }
}

int dispatcher::make_socket_nonblocking(int sockfd) {
    int flags;

    flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl() - F_GETFL");
        return -1;
    }

    flags = flags | O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) < 0) {
        perror("fcntl() - F_SETFL");
        return -1;
    }

    return 0;
}
