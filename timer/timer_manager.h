#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H


#include"timing_wheel.h"
#include"../service/http_conn.h"
#include<sys/mman.h>
class  timing_wheel ; 

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int interval );

    // 对文件描述符设置非阻塞
    int setnonblocking(int fd);

    // 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    // 信号处理函数 应该仅有两种信号
    static void sig_handler(int sig);

    // 设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    // 定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler(timing_wheel *timer_manager, std::unordered_map<int , std::shared_ptr<http_connection>> &users , int user_count) ; 

    void show_error(int connfd, const char *info);

    static bool mod_fd(int sock_fd ,int old_events  , int new_events) ; 

    //对文件进行内存映射
    void* u_mmap(int fd, int len, int offset );

    bool  u_unmmap(char* buffer , int len );

public:
    static int *u_pipefd;
    static int u_epollfd;
    int m_INTERVAL;  //计时周期
};


#endif