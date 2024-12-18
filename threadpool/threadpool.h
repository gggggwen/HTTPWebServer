#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<memory>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<assert.h>
#include<iostream>
#include<sstream>
#include<thread>
#include<list>
#include<vector>
#include"../ipc/lock.h"
#include"../ipc/fifo.h"
#include"../CGImysql/cgimysql.h"
#include"../timer/timer_manager.h"

class Sqlconnect_pool;
class Utils ;
class Webserver ;  

template<typename T>
class threadpool
{
    friend class Webserver ;
public:
    threadpool( ) ;
    void init(int task_num, int thread_num, Sqlconnect_pool *connection_pool);
    ~threadpool() ; 
    static void worker(void* args) ; //内部调用run  
    void run() ;    //线程真正的工作函数
    bool append(std::shared_ptr<T> &job); // 在请求队列中追加http_conn类 由主线程使用 , 在结束记得post 唤醒工作线程
private:
    Semaphore m_queuesem ;
    Mutexlock m_queuelock ;
    Mutexlock m_dead_userslock;

    std::list<std::shared_ptr<T> > m_dead_users ; 
    std::list<std::shared_ptr<T> > m_jobqueue  ;
    std::vector<std::thread> m_threads;
    Sqlconnect_pool *m_connPool; // 数据库

    int m_max_task   ;
    int m_thread_num ;

};

template <typename T>
threadpool<T>::threadpool()
{
    m_max_task = 0;
    m_thread_num = 0;
    m_connPool = nullptr;
}

template <typename T>
void threadpool<T>::init(int task_num, int thread_num, Sqlconnect_pool *connPool)
{
    assert(thread_num > 0 && task_num > 0);
    assert(connPool);

    m_max_task = task_num;
    m_thread_num = thread_num;
    m_connPool = connPool;

    for (int i = 0; i < thread_num; i++)
    {
        m_threads.emplace_back(threadpool::worker, this); // c++11禁用了拷贝
        m_threads.back().detach();
    }
    return ; 
}

template <typename T>
threadpool<T>::~threadpool()
{
    if (m_connPool)
    {
        m_connPool->destroyPool() ; 
    }
    if (!m_jobqueue.empty()) // 防止出现迭代失效
    {
        for (auto it = m_jobqueue.begin(); it != m_jobqueue.end();)
        {
            it = m_jobqueue.erase(it); 
            /*无需手动delete，std::shared_ptr 会自动释放资源*/
        }
    }
}

template <typename T>
void threadpool<T>::worker(void *args)
{
    threadpool<T> *pool = reinterpret_cast<threadpool<T> *>(args);
    pool->run();
}

template <typename T>
void threadpool<T>::run()
{
    // /*create fifo*/
    // std::thread::id thread_id = std::this_thread::get_id();
    // std::stringstream ss;
    // ss << thread_id;
    // std::string str = "/tmp/fifo."+ss.str();
    // std::cout<< str<< std::endl ;

    //RD_Fifo fifo(str.c_str()) ;

    while (true)
    {
        std::shared_ptr<T> user ; 
        m_queuesem.wait();
        {
            MutexLockguard lockguard(m_queuelock);
            if (m_jobqueue.empty())
                continue;
            user = m_jobqueue.back() ; 
            m_jobqueue.pop_back();
        }
        if(user.get()->process())
        {
            Utils::mod_fd(user->m_sockfd, EPOLLET | EPOLLIN | EPOLLRDHUP, EPOLLOUT);
            continue ; 
        }

        {
            MutexLockguard lockguard(m_dead_userslock) ; 
            m_dead_users.push_back(user) ; 
        }
    }
}

template <typename T>
bool threadpool<T>::append(std::shared_ptr<T>& job)
{
    if (!job.get())
        return false;

    MutexLockguard lockguard(m_queuelock);
    if (m_jobqueue.size() >= m_max_task)
        return false;

    m_jobqueue.push_front(job);
    m_queuesem.post();
    return true;
}

#endif