#include"config.h"

Config::Config()
{
    /*端口号*/
    PORT = 9099 ; 

    /*开辟的线程数*/
    THREAD_NUM = 8 ; 

    //监听套接字 默认ET 
    LISTEN_MOD =  1 ; 

    //负责用户连接 默认ET ; 
    CONNECT_MOD =  1 ; 

    /*最大任务数*/
    MAX_TASK = 5000 ; 

    /*日志模式 默认异步*/
    LOG_WRITE = 1 ;

    /*日志存放地址*/
    LOG_DIRECTORY = "/home/gwen/github_repo/MyTinyWebServer/logdata/";

    /*tick的周期*/
    INTERVAL = 2 ; 

    /*时间轮槽数*/
    SLOT_NUM = 3 ; 

    /*数据库接口信息*/
    SQL_CONNECTION_NUM = 8;
    SQL_PORT = 3306 ; 

    /*数据库管理信息*/
    NAME  = "gwen" ;
    PASSWD = "126943" ; 
    DBNAME = "db" ; 
    URL = "localhost" ; 
}

void 
Config::parse_arg(int argc , char* argv[]) 
{
    int opt;
    char* optarg ; 
    const char *str = "p:l:m:s:t:";
    while ((opt = getopt(argc, argv, str)) != -1)
    {
        switch (opt)
        {
        case 'p':
        {
            PORT = atoi(optarg);
            break;
        }
        case 'l':
        {
            LOG_WRITE = atoi(optarg);
            break;
        }
        case 'm':
        {
            CONNECT_MOD = atoi(optarg) ;
            LISTEN_MOD = atoi(optarg) ; 
            break;
        }
        case 's':
        {
            SQL_CONNECTION_NUM = atoi(optarg);
            break;
        }
        case 't':
        {
            THREAD_NUM = atoi(optarg);
            break;
        }
        default:
            break;
        }
    }
    return ; 
}