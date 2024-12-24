#ifndef IO_THREAD_H
#define IO_THREAD_H

#include"../log/log.h"
#include"../ipc/lock.h"
#include"../timer/timer_manager.h"
#include<iostream> 
#include <stdio.h>
#include <string>
#include <list>
#include <memory>
#include <thread>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/errno.h>
#include <unistd.h>

#ifndef MAX_IO_QUEUE_SIZE
#define MAX_IO_QUEUE_SIZE 1000
#endif

#define BUFFER_SIZE 1024

class Utils;
class Webserver;

template<typename T>
class O_thread 
{
public:
    friend class Webserver ; 
    O_thread(){} ; 
    ~O_thread(){} ; 
    void run() ;
    void handle(std::shared_ptr<T> user_sp);
    bool append(std::shared_ptr<T> user_p);
    void init(int connect_mod);
    static void worker(void *args);
private:
    

private:
    Utils m_utils; 
    int   m_connect_mod ;
    std::list<std::shared_ptr<T>> m_users_queue;
    std::list<int> m_dead_users;
    Semaphore m_queuesem;
    Mutexlock m_queuelock;
    Mutexlock m_dead_userslock;
    std::thread m_thread;
};

template <typename T>
void O_thread<T>::worker(void *args)
{
    O_thread<T> *thread = reinterpret_cast<O_thread<T> *>(args);
    thread->run();
}


template <typename T>
bool O_thread<T>::append(std::shared_ptr<T> user_p)
{
    if (!user_p.get())
        return false;

    MutexLockguard lockguard(m_queuelock);
    if (m_users_queue.size() >= MAX_IO_QUEUE_SIZE)
        return false;

    m_users_queue.push_front(user_p);
    m_queuesem.post();
    return true;
};

template <typename T>
void O_thread<T>::init(int connect_mod)
{
    m_connect_mod = connect_mod;
    std::thread tmp_thread(O_thread<T>::worker, this);
    m_thread = std::move(tmp_thread);
    m_thread.detach();
};

template <typename T>
void O_thread<T>::handle(std::shared_ptr<T> user_sp)
{
    T *user = user_sp.get();
    int sockfd = user->m_sockfd;
    int bytes_need_send = user->write_offset;
    char *buffer = user->write_buffer;

    while (bytes_need_send > 0)
    {
        int temp = write(sockfd, buffer, bytes_need_send);
        if (temp > 0)
        {
            bytes_need_send -= temp;
        }
        else if (temp < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            else
            {
                if (errno == EBADF)
                {
                    LOG_ERROR("The sockfd %d was closed before send data to client", user->m_sockfd);
                }
                else
                {
                    LOG_ERROR("Send error , Uable to send http datagram in sockfd %d", user->m_sockfd);
                }
                m_dead_users.push_front(sockfd);
                return;
            }
        }
        else
        {
            break;
        }
    }

    if (user->file_path.length() > 0)
    {
        LOG_INFO("the file %s will be send to client", user->file_path.c_str());
        int file_size = user->get_file_size() ; 
        int fd = open(user->file_path.c_str()  ,O_RDONLY , 0 ) ;
        if(file_size>BUFFER_SIZE*10)
        {
            int  bytes_read = 0;
            char *buffer =(char*) m_utils.u_mmap(fd, file_size, 0);
            close(fd);

            if((void*)buffer == MAP_FAILED )
            {
                m_dead_users.push_front(sockfd);
                m_utils.u_unmmap(buffer , file_size) ;
                LOG_ERROR("Map failed ,Uable to send file in sockfd %d", user->m_sockfd);
                return ;
            }

            while(bytes_read < file_size)
            {
                size_t chunk_size = std::min(BUFFER_SIZE, file_size - bytes_read);
                if(send(sockfd , buffer+bytes_read , chunk_size , 0 ) ==-1)
                {
                    m_dead_users.push_front(sockfd);
                    m_utils.u_unmmap(buffer, file_size);
                    if(errno == EBADF)
                    {
                        LOG_ERROR("The sockfd %d was closed before send data to client", user->m_sockfd);
                    }
                    else 
                    {
                        LOG_ERROR("Send error , Uable to send file in sockfd %d", user->m_sockfd);
                    }
                    return ; 
                }
                bytes_read+=chunk_size ; 
            }
            m_utils.u_unmmap(buffer, file_size);
        }
        else 
        {
            char* buffer = new char[BUFFER_SIZE] ; 
            size_t bytes_read;
            while ((bytes_read = read(fd , buffer , BUFFER_SIZE)) > 0)
            {
                if (send(sockfd, buffer , BUFFER_SIZE , 0) == -1)
                {
                    m_dead_users.push_front(sockfd);
                    delete[] buffer;
                    close(fd);
                    LOG_ERROR("Send error , Uable to send file in sockfd %d", user->m_sockfd);
                    return;
                }
            }
            delete[] buffer ; 
            close(fd);
        }
    }
    else
        LOG_INFO("no file need to be transmit... maybe some error occurred");

    /*完成数据传输后若需要关闭套接字则直接关闭即可*/
    if (user->close_conn)
    {
        m_dead_users.push_front(sockfd);
    }

    /*清空buffer*/
    user->clear_data();
    /*完成写事件后 , 注销写事件 */
    if (m_connect_mod)
        m_utils.mod_fd(sockfd, EPOLLET | EPOLLIN | EPOLLRDHUP, 0);
    else
        m_utils.mod_fd(sockfd, EPOLLIN | EPOLLRDHUP, 0);
    return;
};

template <typename T>
void O_thread<T>::run()
{
    while (true)
    {
        m_queuesem.wait();
        {
            MutexLockguard lockguard(m_queuelock);
            if (m_users_queue.empty())
                continue;
            handle(m_users_queue.back());
            m_users_queue.pop_back();
        }
    }
};

#endif 