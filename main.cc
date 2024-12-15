#include<iostream> 
#include"config.h"
#include"webserver.h"


int main(int argc , char** argv)
{
    // 获取指令参数
    Config cfg;
    cfg.parse_arg(argc, argv); 

    Webserver server ;
    server.init(cfg.PORT, cfg.THREAD_NUM, cfg.SQL_CONNECTION_NUM, cfg.LISTEN_MOD, cfg.CONNECT_MOD,
                cfg.LOG_WRITE, cfg.INTERVAL, cfg.SLOT_NUM, cfg.MAX_TASK, cfg.SQL_PORT, cfg.LOG_DIRECTORY,
                cfg.NAME, cfg.PASSWD, cfg.DBNAME, cfg.URL);

    server.log_init() ; 

    server.pool_init() ;

    server.timer_manager_init();

    server.event_listen() ;

    std::cout<<"ther server is running!"<<std::endl;
    server.event_loop() ; 

    return 0 ; 
};