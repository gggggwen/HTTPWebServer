#ifndef WEBSERVER_H
#define WEBSERVER_H

#include<unordered_map>
#include<string>
#include<unistd.h>
#include<errno.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include <mysql/mysql.h>

#include"./timer/timer_manager.h"
#include"config.h"
#include"./threadpool/threadpool.h"
#include"./CGImysql/cgimysql.h"
#include"./service/http_conn.h"

class Utils ;
class timing_wheel ; 


class Webserver
{
public:
    Webserver();
    ~Webserver();
    void init(int port, int thread_num, int sql_connection_num, int listen_mod,
              int connect_mod, int log_write, int interval, int slot_num, int max_task, int mysql_port,
              char *log_directory, std::string name, std::string passwd, std::string dbname, std::string url);
    //初始化日志系统
    void log_init() ; 
    //初始化线程池 , 数据库连接池
    void pool_init() ;
    //初始化计时管理器
    void timer_manager_init() ; 
    //初始化监听套接字
    void event_listen() ;
    //监听注册的套接字
    void event_loop() ;  

private:
    bool handle_read(int sockfd, char *buffer );
    bool handle_write(int sockfd, char *buffer, int &bytes_need_send);
    bool handle_new_connection() ; 
    bool handle_dead_connection(int sockfd) ;
    bool handle_signal(bool& timeout ) ; 
public:
    static const int MAX_EVENTS_NUM =10000 ; 
    static const int MAX_CONNECTION = 5000 ; 
    Utils m_utils ; 
    int* m_pipe ;
    int m_epfd;
    static int m_user_count ; 

private:
    sockaddr_in m_servaddr ;
    epoll_event events[MAX_EVENTS_NUM];
    int m_servsock;

    int m_port ;
    int m_thread_num ; 
    int m_sql_num ; 
    int m_listen_mod ;
    int m_connect_mod ;
    int m_log_write ; 
    int m_interval  ; 
    int m_slot_num ; 
    char* m_log_directory;

    std::string m_name ; 
    std::string m_passwd;
    std::string m_dbname ; 
    std::string m_url ; 
    int m_mysql_port ; 
    
    std::unordered_map<int, std::shared_ptr<http_connection>> m_users ; //可通过套接字查询

    //有关线程池
    int m_max_task; //请求队列最大容量
    threadpool<http_connection> m_threadpool ;

    timing_wheel* m_timer_manager ; 

};

#endif 