#include"cgimysql.h"

Sqlconnect_pool*
Sqlconnect_pool::get_instance()
{
    static Sqlconnect_pool pool ; //构造函数初始化 ,但是这种情况可以调用
    return &pool ; 
}

int 
Sqlconnect_pool::freeConn_count()
{
    return m_freeConn ; 
}

void
Sqlconnect_pool::init(std::string Url, std::string Name, std::string Passwd, std::string DBname , int port, int maxconn)
{
    assert(port >= 0 && Url.length() != 0 && maxconn > 0);

    m_url = Url ;
    m_name = Name ; 
    m_passwd = Passwd ; 
    m_dbname = DBname ;
    m_port = port ; 
    m_maxConn = maxconn ; 
    m_freeConn = maxconn ; 

    m_sem = Semaphore(maxconn) ; 
    for(int i = 0 ; i <maxconn ; i++)
    {
        MYSQL* connection = nullptr  ;
        connection = mysql_init(connection) ; 
        if(!connection)
        {
            LOG_ERROR("mysql connection initialization failed !") ; 
            exit(1) ; 
        }
        
        connection = mysql_real_connect(connection , Url.c_str(), Name.c_str() , Passwd.c_str() , DBname.c_str() , port , NULL , 0 ) ;
        if (!connection)
        {
            LOG_ERROR("mysql connection initialization failed !");
            exit(1);
        }
        
        m_pool.push_front(connection) ; 
    }
}

MYSQL*
Sqlconnect_pool::getConnection()
{
    MYSQL *res = nullptr ;
    m_sem.wait();
    {
        MutexLockguard lockguard(m_mutex);

        if (m_freeConn == 0)
        {
            LOG_WARN("mysql connection is busy , one thread can not get an connection source");
            return nullptr ;                                                                                
        }
        m_freeConn--;
        res = m_pool.back();
        m_pool.pop_back();
    }
    assert(res); 
    return res ; 
    
}

bool 
Sqlconnect_pool::releaseConnection(MYSQL* connection)
{
    if(!connection)
    {
        LOG_WARN("Attempting to reclaim an empty connection into the mysql pool") ; 
        return false ; 
    }
    {
        MutexLockguard lockguard(m_mutex) ; 
        m_pool.push_front(connection) ; 
        m_freeConn ++;
    }
    m_sem.post() ; 
    LOG_DEBUG("reclaim an connection into the mysql pool successfullly");
    return true ; 
}

void 
Sqlconnect_pool::destroyPool()
{   
    m_mutex.lock() ; 
    for(auto it = m_pool.begin() ; it!=m_pool.end() ;  )
    {
        MYSQL* temp = *it ; 
        it = m_pool.erase(it) ;
        if (temp)
        {
            mysql_close(temp); // 使用 MySQL API 释放连接
        }
    }
    m_mutex.unlock() ; 
}