#include"timer_manager.h"

int *Utils::u_pipefd = nullptr;
int Utils::u_epollfd = 0;


void
Utils::init(int interval )
{
    m_INTERVAL = interval ; 
}

int 
Utils::setnonblocking(int fd)
{
    int old_opt = fcntl(fd , F_GETFD) ;
    int new_opt = old_opt| O_NONBLOCK;
    if (fcntl(fd, F_SETFL, new_opt) < 0)
    { // 设置文件状态标志
        LOG_ERROR("fcntl set error");
        return -1;
    }
    return old_opt ; 
}

void
Utils::addfd(int epollfd , int fd , bool one_shot , int TRIGMode)
{
    epoll_event event ; 
    event.data.fd = fd ;
    if(TRIGMode==1 ) //ET
    {
        event.events = EPOLLET | EPOLLIN | EPOLLRDHUP ;
    }
    else 
    {
        event.events = EPOLLIN | EPOLLRDHUP ;
    }
    if(one_shot)
    {
        event.events |=EPOLLONESHOT ;
    }
    epoll_ctl(epollfd , EPOLL_CTL_ADD , fd ,&event) ; 
    return ; 
}

void
Utils::sig_handler(int sig)
{
    // 为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void
Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::timer_handler(timing_wheel *timer_manager, std::unordered_map<int, std::shared_ptr<http_connection>>& users, int user_count)
{
    timer_manager->tick(users  , user_count ,  Utils::u_epollfd );
    alarm(m_INTERVAL);
}

void 
Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

bool 
Utils::mod_fd(int sockfd , int old_events , int new_events)
{
    struct epoll_event event ; 
    event.data.fd = sockfd ; 
    event.events = old_events|new_events ; 

    int res = epoll_ctl(Utils::u_epollfd , EPOLL_CTL_MOD , sockfd , &event) ; 
    if(res == -1 )
    {
        LOG_ERROR("epoll _ctl modify error about sockfd:%d!" , sockfd ) ;
        return false ;
    }
    return true ;
}

