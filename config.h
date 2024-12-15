#ifndef CONFIG_H 
#define CONFIG_H

#include<string>
#include<unistd.h>


class Config
{
public:
    Config()  ; 
    ~Config(){} ; 

    void parse_arg(int argc , char* argv[]);

public:
    int PORT ;

    int THREAD_NUM ; 
    
    int LISTEN_MOD; 

    int CONNECT_MOD ; 

    int INTERVAL ; 

    int SLOT_NUM ; 

    int MAX_TASK ;

    int LOG_WRITE ;
    char* LOG_DIRECTORY ;

    int SQL_PORT;
    int SQL_CONNECTION_NUM;
    std::string  NAME;
    std::string  PASSWD ;
    std::string  DBNAME;
    std::string  URL ; 
};



#endif