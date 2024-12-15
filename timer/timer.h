#ifndef TIMER_H
#define TIMER_H

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <vector>
#include <memory>

struct timer 
{
    sockaddr_in addr;
    int sockfd;
};
#endif 