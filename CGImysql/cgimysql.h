// 1.单例模式
// 2.具体的业务逻辑可以自行配置 仅负责分配和挥手连接

#ifndef CGIMYSQL_H
#define CGIMYSQL_H

#include<mysql/mysql.h>
#include<iostream>
#include<string>
#include<list>
#include"../lock/lock.h"
#include"../log/log.h"

class Sqlconnect_pool
{
public:
    static Sqlconnect_pool* get_instance() ;
    inline int freeConn_count();
    void init(std::string Url, std::string Name, std::string Passwd, std::string DBname, int port, int maxconn);
    MYSQL*   getConnection() ;  //可能返回空指针注意
    bool  releaseConnection(MYSQL* connection) ; 
    void   destroyPool() ; 

private : 
    Sqlconnect_pool()
    {
        m_freeConn = 0 ;
        m_maxConn =  0 ;
    }
    ~Sqlconnect_pool()
    {
        this->destroyPool() ; 
    } ; 

public:
    int m_freeConn ; 
    int m_maxConn  ;

private:
    std::string m_url;      //不太清楚
    std::string m_name ;    //数据库管理者名称
    std::string m_passwd ; //数据库,密码
    std::string m_dbname ; //数据库名称 
    int m_port;
    std::list<MYSQL*> m_pool ; 
    Mutexlock  m_mutex ; 
    Semaphore  m_sem ; 
};

class sql_connection  //RAII
{
public:
    sql_connection()
    {
        RAIIpool = Sqlconnect_pool::get_instance() ; 
        RAIIconnection = RAIIpool->getConnection() ; 
    }
    ~sql_connection()
    {
        RAIIpool->releaseConnection(RAIIconnection) ;  //释放连接
    }
    MYSQL* get_connection()
    {
        return RAIIconnection ; 
    }
private:
    Sqlconnect_pool* RAIIpool ;
    MYSQL *RAIIconnection;
};
#endif