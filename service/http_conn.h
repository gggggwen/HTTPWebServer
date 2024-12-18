#ifndef HTTP_CONN_H 
#define HTTP_CONN_H

#include <mysql/mysql.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream> 
#include <string>
#include <map>
#include"../log/log.h"
#include"../CGImysql/cgimysql.h"

#ifndef READ_BUFFER_SIZE
#define READ_BUFFER_SIZE 4096
#endif

#ifndef WRITE_BUFFER_SIZE
#define WRITE_BUFFER_SIZE 4096
#endif

#ifndef DIR_MODE
#define DIR_MODE S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH
#endif  

class http_connection
{
    enum METHOD
    {
        GET = 0,
        POST   ,
        DELETE ,
        UNKOWN , 
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER  ,
        CHECK_STATE_CONTENT ,
        DONE , 
    };
    enum HTTP_CODE
    {
        GET_REQUEST = 0,
        BAD_REQUEST ,
        NO_REQUEST  ,
        NO_RESOURCE , //404
        FORBIDDEN_REQUEST, //403
        INTERNAL_ERROR   , //500
        CLOSED_CONNECTION,
        METHOD_NOT_ALLOWED,
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
    enum ACTION
    {
        G_REGISTER_HTML = 0,
        G_LOGIN_HTML   ,
        REGISTER ,
        LOGIN    ,
        UPDATE_FILE ,
        GET_FILE ,
        GET_DIR  , 
        DELETE_FILE , 
        UNDEFINED ,
    };
public:
    bool process();
    void init(int sockfd);
    void clear_data() ; 
private:
    /*tools*/
    bool  file_available() ;
    bool  dir_available()  ;
    std::string  get_threadID_s() ; 
    std::string  get_file_path() ; 
    std::string  get_dir_path()  ;
    void  decode_path(std::string & path) ; 
    bool  get_dir_content() ; 
    int   get_file_size() ;
    bool  get_user_passwd() ;
    bool  mysql_process(); 
    bool  analyze_get()  ;
    bool  analyze_post() ;
    bool  analyze_del () ; 
    /*解析请求报文*/
    LINE_STATUS parse_line(); // 逐条分析http报文格式是否合法
    bool  parse_request_line() ; 
    bool  parse_request_header() ;
    bool  parse_request_body() ;
    bool  do_request();
    HTTP_CODE process_read() ; 

    /*生成响应报文*/
    void generate_response_line() ;
    void generate_response_header() ; 
    void generate_response_body() ; 
    bool process_write() ; 

public:
    static std::string accept_language ;
    std::string file_path ; 
    std::string dir_path ; 
    char* read_buffer ;
    int read_offset ;  
    char* write_buffer ; 
    int  write_offset ;
    bool close_conn ; 
    int m_sockfd;
private:
    HTTP_CODE   m_http_code ; 
    CHECK_STATE m_check_state ; 
    METHOD      m_method  ;
    ACTION      m_action ;  
    std::string user ;
    std::string passwd ;    
};

#endif