/**
 * 该日志类必须满足的1.线程安全 ， 2.整个程序生命周期中有且仅有一个实例 , 3.提供同步和异步两种模式 4.要求日志输入是毫秒级 ,或是微秒级
 * 提供接口: 分为四个日志等级  INFO DEBUG WARNING ERROR , 接口都采用宏定义 , 需要首先获取实例, 随后进行write_log 操作
 * 具体实现:
 *  1. 日志格式 : 日志等级 时间+微妙 线程 正文 (源文件名+行号)
 *  2. 使用两个队列 / C风格字符存放消息
 *  ....
 *
 *  考虑如下特殊情况:
 *      1. 当有消息需要插入队列时 , 队列恰好塞满 :
 *      2. 当一组队列恰在进行写入日志操作 , 而另一组也已满;
 **/

#ifndef LOG_H 
#define LOG_H 

#include<unistd.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include<sys/time.h>
#include<cstring>
#include<string> 
#include<list>
#include<string.h>
#include<fstream>
#include<iostream>
#include "../lock/lock.h"

#define BUFFERSIZE  4*1024*1024 


typedef struct Buffer 
{
    char buffer[BUFFERSIZE]  ;
    int   ptr       ;
}Buffer;

typedef struct Buffer* BufferPtr ; 


class Log
{
public:

    static Log* get_instance() 
    {
        static Log object ;
        return &object ;  
    }
    void init(const char *directory, int is_async);
    void write_log(int log_level, const char *str,...); // 需要传入参数 看看情况
    void async_write_log() ; 
    static void  write_log_thread()
    {
        Log::get_instance()->async_write_log() ; 
    }
    
private:
    //因为Log采用单例模式进行设计 所以需要将构造函数 , 析构函数私有化
    BufferPtr  currentBuffer_ ;
    BufferPtr  backstageBuffer_;
    BufferPtr  readyBuffer_;
    int m_is_async;
    struct tm * m_log_tm ; 
    std::string  m_directory ;
    std::string  m_filename  ;
    std::ofstream m_log_file ;  //写文件类
    Mutexlock mutex_ ;
    Condition cond_ ;
    
    inline void swap_buffer(BufferPtr& buffer_a , BufferPtr& buffer_b) noexcept; // 交换buffer ,,,不可被外界调用
    Log (){} ;
    ~Log(){} ;

};


#define LOG_DEBUG(format, ...)  Log::get_instance()->write_log(0, format, ##__VA_ARGS__);
#define LOG_INFO(format, ...)  Log::get_instance()->write_log(1, format, ##__VA_ARGS__); 
#define LOG_WARN(format, ...)  Log::get_instance()->write_log(2, format, ##__VA_ARGS__); 
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__); 

#endif