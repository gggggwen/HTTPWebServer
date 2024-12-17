#ifndef LOCK_H
#define LOCK_H
#include <pthread.h>
#include<semaphore.h>
#include <iostream>
#include <assert.h>
#include <thread>
#include<exception>

class Semaphore
{
private:
    sem_t sem_ ;  
public:
    Semaphore()
    {
        if(sem_init(&sem_, 0 , 0 )==-1)
        {
            throw std::exception() ;  
        }

    }
    Semaphore(int num) 
    {
        if(sem_init(&sem_ , 0 , num) ==-1) 
        {
            throw std::exception();
        } 
    }
    ~Semaphore()
    {
        if(sem_destroy(&sem_)==-1)
        {
            throw std::exception() ; 
        }   
    }
    bool wait()
    {
        return sem_wait(&sem_)==0 ;
    }
    bool post()
    {
        return sem_post(&sem_) == 0;
    }
};

class Mutexlock
{
private:
    pid_t holder_;
    pthread_mutex_t mutex_;

public:
    Mutexlock() : holder_(0)
    {
        pthread_mutex_init(&mutex_, nullptr);
    }
    ~Mutexlock()
    {
        assert(holder_ == 0);
        pthread_mutex_destroy(&mutex_);
    }
    bool isLocked_by_thisThread()
    {
        return holder_ == pthread_self();
    }

    void lock()
    {
        pthread_mutex_lock(&mutex_);
        holder_ = pthread_self();
    }

    void unlock()
    {
        holder_ = 0;
        pthread_mutex_unlock(&mutex_);
    }

    pthread_mutex_t *getPthreadmutex()
    {
        return &mutex_;
    }
};

class MutexLockguard
{
public:
    MutexLockguard(Mutexlock &mutex) : mutex_(mutex)
    {
        mutex_.lock();
    }
    ~MutexLockguard()
    {
        mutex_.unlock();
    }

private:
    Mutexlock &mutex_;
};


class Condition
{
private:
    pthread_cond_t condition_variable_;

public: 
    Condition() 
    {
        pthread_cond_init(&condition_variable_, nullptr) ;
    }
    ~Condition()
    {
        pthread_cond_destroy(&condition_variable_);
    }
    bool wait(Mutexlock& mutex)
    {
        int res ; 
        res = pthread_cond_wait(&condition_variable_, mutex.getPthreadmutex());
        return res == 0;
    }
    bool timewait(Mutexlock& mutex, struct timespec t)
    {
        int res = 0;
        res = pthread_cond_timedwait(&condition_variable_, mutex.getPthreadmutex(), &t);
        return res == 0;
    }
    void notify_one()
    {
        pthread_cond_signal(&condition_variable_);
    }
    void notify_all()
    {
        pthread_cond_broadcast(&condition_variable_);
    }

};



#endif
