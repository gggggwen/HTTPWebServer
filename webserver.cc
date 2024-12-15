#include"webserver.h"
int Webserver::m_user_count = 0 ; 

#ifndef READ_BUFFER_SIZE
#define READ_BUFFER_SIZE 4096
#endif

#ifndef WRITE_BUFFER_SIZE
#define WRITE_BUFFER_SIZE 4096
#endif
Webserver::Webserver()
{
    m_port = 0;
    m_thread_num = 0;
    m_sql_num = 0;
    m_listen_mod = 0;
    m_connect_mod = 0;
    m_log_write = 0;
    m_log_directory = 0;
    m_max_task = 0 ;
    m_interval = 0;
    m_slot_num = 0;
    
    m_mysql_port = 0 ; 
    m_pipe = nullptr;
    m_timer_manager = nullptr;
    m_epfd = 0;

    memset(&m_servaddr , 0 , sizeof(m_servaddr)) ; 
};

void 
Webserver::init(int port, int thread_num, int sql_connection_num, int listen_mod, 
             int connect_mod, int log_write, int interval , int slot_num , int max_task , int mysql_port ,
             char *log_directory, std::string name, std::string passwd, std::string dbname, std::string url)
{
    m_port = port ; 
    m_thread_num = thread_num ; 
    m_sql_num = sql_connection_num ; 
    m_listen_mod = listen_mod ;
    m_connect_mod = connect_mod ; 
    m_log_write = log_write ; 
    m_log_directory = log_directory ; 
    m_interval = interval ; 
    m_slot_num = slot_num ;

    m_max_task = max_task ; 
    m_dbname  = dbname ;
    m_name  = name ; 
    m_passwd = passwd ; 
    m_url = url ;
    m_mysql_port = mysql_port ; 
};


Webserver::~Webserver()
{
    delete[] m_pipe;
    m_pipe = nullptr;

    delete m_log_directory;
    m_log_directory = nullptr;
};

void 
Webserver::log_init()
{
    Log::get_instance()->init(m_log_directory, m_log_write );
    LOG_INFO("establish log successfully!");
    return ; 
};

void 
Webserver::pool_init()
{
    /*初始化数据库连接池*/
    Sqlconnect_pool::get_instance()->init(m_url , m_name , m_passwd , m_dbname , m_mysql_port,m_sql_num ) ;
    LOG_INFO("establish MYSQL connection pool successfully!");

    m_threadpool.init(m_max_task , m_thread_num ,Sqlconnect_pool::get_instance()) ;
    LOG_INFO("establish threadpool successfully!") ; 
    return ; 
};

void 
Webserver::timer_manager_init()
{
    m_timer_manager = new timing_wheel ; 

    m_utils.init(m_interval) ; 
    m_timer_manager->init(m_interval , m_slot_num) ; 
    LOG_INFO("establish timer manager successfully!") ; 
    return ; 
};

bool 
Webserver::handle_new_connection()
{
    struct sockaddr_in clnt_addr ;
    socklen_t adr_sz = sizeof(clnt_addr);
    int sockfd ; 
    if(m_listen_mod == 0 )
    {
        sockfd = accept(m_servsock, (struct sockaddr *)&clnt_addr, &adr_sz);
        if(sockfd< 0 )
        {
            m_utils.show_error(sockfd, "Internal server busy");
            LOG_ERROR("Internal server busy");
            return false;
        }
        m_utils.setnonblocking(sockfd); // 设置非阻塞

        if (m_connect_mod == 1) // ET
        {
            m_utils.addfd(m_epfd, sockfd, false, m_connect_mod);
        }
        else // LT
        {
            m_utils.addfd(m_epfd, sockfd, true, m_connect_mod);
        }

        timer *new_timer = new timer;
        new_timer->sockfd = sockfd;
        new_timer->addr = clnt_addr;   
        m_timer_manager->add_timer(new_timer);   //创建计时器

        std::shared_ptr<http_connection> new_user = std::make_shared<http_connection>() ;
        new_user.get()->init(sockfd) ; //初始化函数后续还需要修改注意一下!!!          
        m_users.insert({sockfd, new_user});  //插入至对象池中
        Webserver::m_user_count++;
        LOG_INFO("sockfd: %d new connection established !", sockfd);
    }
    else 
    {
        while(true)
        {
            sockfd = accept(m_servsock, (struct sockaddr *)&clnt_addr, &adr_sz);
            if(sockfd < 0 )
            {
                return false ; 
            }
            if (Webserver::m_user_count >= MAX_CONNECTION)
            {
                m_utils.show_error(sockfd, "Internal server busy");
                LOG_WARN( "Internal server busy");
                //这里需要对客户端进行响应
                return false ; 
            }
            if(m_connect_mod == 1) //ET
            {
                m_utils.addfd(m_epfd, sockfd, false , m_connect_mod);
            }
            else  //LT
            {
                m_utils.addfd(m_epfd, sockfd, true , m_connect_mod);
            }
            m_utils.setnonblocking(sockfd); // 设置非阻塞

            timer *new_timer = new timer;
            new_timer->sockfd = sockfd;
            new_timer->addr = clnt_addr;
            m_timer_manager->add_timer(new_timer); // 创建计时器

            std::shared_ptr<http_connection> new_user = std::make_shared<http_connection>();
            new_user.get()->init(sockfd);                 // 后续可能需要修改
            m_users.insert({sockfd, new_user}); // 插入至对象池中
            Webserver::m_user_count++;
            LOG_INFO("new connection established !");
        }
    }
    return true ; 
};

bool 
Webserver::handle_dead_connection(int sockfd) 
{
    timer* temp = m_timer_manager->del_timer(sockfd ) ; //从计时器中删除
    delete temp ;   //释放计时器

    m_users.erase(sockfd) ;
    epoll_ctl(m_epfd , EPOLL_CTL_DEL , sockfd , nullptr) ;
    close(sockfd) ;
    Webserver::m_user_count--;
    return true ; 
};

//处理读事件啊+关闭连接请求
bool 
Webserver::handle_read(int sockfd , char* buffer  ) 
{
    int bytes_have_read = 0 ; 
    http_connection*  user = m_users[sockfd].get() ;
    if(!user)
    {
        LOG_ERROR("the user of sockfd:%d does not exist!", sockfd) ; 
        return false ; 
    } 
    if (m_connect_mod == 1) // ET
    {
        while(true) 
        {
            int temp = read(sockfd , buffer+bytes_have_read ,READ_BUFFER_SIZE ) ; 
            if( temp> 0)
            {
                std::cout<<buffer<<std::endl;
                std::cout<<"--------------------"<<std::endl;
                bytes_have_read +=temp ; 
                if(bytes_have_read>=READ_BUFFER_SIZE)
                    break ; 
            }
            else if(temp == 0 )
            {
                handle_dead_connection( sockfd) ; 
                return true ;  
            }
            else 
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)  
                {
                    break;
                }
                else 
                {
                    LOG_ERROR("unkown error occured in connect socket ");
                    handle_dead_connection( sockfd) ;        
                    return false;
                }
            }
        } 
        m_timer_manager->adjust_timer(sockfd) ;
        m_threadpool.append(m_users[sockfd]);
    }
    else // LT 
    {
        int temp = read(sockfd, buffer, READ_BUFFER_SIZE);
        if (temp > 0)
        {
            m_timer_manager->adjust_timer(sockfd);
            return false ; 
        }
        else if (temp == 0)
        {
            handle_dead_connection(sockfd);
            return true ; 
        }
        else
        {   
            m_timer_manager->adjust_timer(sockfd);
            m_threadpool.append(m_users[sockfd]);
            return true ; 
        }
    }
    return true;
} ;

//只负责传输http响应报文
bool
Webserver::handle_write(int sockfd , char* buffer , int& bytes_need_send) 
{
    http_connection *user = m_users[sockfd].get();

    if(!buffer||user->write_offset==0 )
    {
        LOG_ERROR("pass in nullptr buffer or zero bytes_need_send! in webserver.cc:264") ; 
        return false ;
    }
    while (bytes_need_send>0 )
    {
        int temp  = write(sockfd , buffer , bytes_need_send) ;
        if(temp > 0 )
        {
            bytes_need_send -= temp;
        }
        else if (temp < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue; 
            else 
            {
                LOG_ERROR("write http daatagram error ! sockfd:%d", sockfd);
                handle_dead_connection(sockfd);
                return false;
            }
        }
        else 
        {
            break ; 
        }
    }


    if(user->file_path.length() > 0 ) 
    {
        LOG_INFO("the file %s will be send to client", user->file_path.c_str()) ; 
        char buffer[1024];
        FILE*  file =fopen(user->file_path.c_str() ,"rb") ;
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
        {
            send(sockfd, buffer, bytes_read, 0);
        }
        fclose(file) ; 
    }
    else 
        LOG_INFO("no file need to be transmit... maybe some error occurred") ; 

    /*完成数据传输后若需要关闭套接字则直接关闭即可*/
    m_timer_manager->adjust_timer(sockfd);
    if (user->close_conn)
    {
        return handle_dead_connection(sockfd);
    }

    /*清空buffer*/
    user->clear_data() ; 
    /*完成写事件后 , 注销写事件 */
    if(m_connect_mod)
        m_utils.mod_fd(sockfd , EPOLLET|EPOLLIN|EPOLLRDHUP , 0) ;
    else 
        m_utils.mod_fd(sockfd , EPOLLIN|EPOLLRDHUP , 0 ) ; 

    return true ; 
};

bool 
Webserver::handle_signal(bool& timeout)
{
    int temp = 0;
    int sig;
    char signals[512];
    temp = recv(m_pipe[0], signals, sizeof(signals), 0);
    if (temp == -1)
    {
        return false;
    }
    else if (temp == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < temp; ++i)
        {
           switch (signals[i])
           {
              case SIGALRM:
                 timeout = true;
                 break;
              default:
                 break ;
           } 
        }
    }
    return true;
};

void 
Webserver::event_listen()
{
    m_servsock = socket(AF_INET , SOCK_STREAM , 0 ) ;
    m_servaddr.sin_addr.s_addr = htonl(INADDR_ANY)  ; 
    m_servaddr.sin_port = htons(m_port) ; 
    m_servaddr.sin_family = AF_INET ;

    //设置套接字端口重用
    int flag = 1;
    setsockopt(m_servsock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    //封装和监听
    int res  ;
    res = bind(m_servsock, (struct sockaddr *)&m_servaddr, sizeof(m_servaddr));
    assert(res!= -1 ) ;
    res = listen(m_servsock , 5) ;
    assert(res != -1);

    m_epfd = epoll_create(5) ; 
    assert(m_epfd != -1 ) ;

    m_utils.setnonblocking(m_servsock) ;   //注意需要设置非阻塞
    m_utils.addfd(m_epfd, m_servsock, false, m_listen_mod);

    m_pipe = new int[2] ; 
    res = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipe);
    assert(res != -1);
    m_utils.setnonblocking(m_pipe[1]);
    m_utils.addfd(m_epfd, m_pipe[0], false, 0);

    m_utils.addsig(SIGPIPE, SIG_IGN);
    m_utils.addsig(SIGWINCH ,SIG_IGN) ; 
    m_utils.addsig(SIGALRM, m_utils.sig_handler, false);

    alarm(m_interval);

    // 工具类,信号和描述符基础操作
    Utils::u_pipefd  = m_pipe;
    Utils::u_epollfd = m_epfd;
};

void
Webserver::event_loop()
{
    bool timeout = false ;
    int res ; 
    while(true)
    {
        int num = epoll_wait(m_epfd , events , MAX_EVENTS_NUM , -1 ) ;
        if (num < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure in webserver.cc 388");
            break;
        }

        for (int i = 0; i < num; i++)
        {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == m_servsock)
            {
                bool flag = handle_new_connection() ; 
                if (false == flag)
                    continue;
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 服务器端关闭连接，移除对应的定时器
                handle_dead_connection(sockfd);
            }
            // 处理信号
            else if ((sockfd == m_pipe[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = handle_signal(timeout);
                if (false == flag)
                    LOG_ERROR("handle_signal failure");
            }
            // 处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                http_connection *user = m_users[sockfd].get();
                handle_read(sockfd, user->read_buffer);
            }
            else if (events[i].events & EPOLLOUT)
            {
                http_connection *user = m_users[sockfd].get();
                LOG_INFO("A write event need to be done in sockfd %d" , sockfd) ; 
                handle_write(sockfd, user->write_buffer, user->write_offset);
            }
        }


        /*关闭不健康的连接*/
        {
            MutexLockguard lockguard(m_threadpool.m_dead_userslock) ; 
            while(m_threadpool.m_dead_users.size()>0)
            {
                http_connection *user = m_threadpool.m_dead_users.back().get();
                handle_dead_connection(user->m_sockfd) ;
                m_threadpool.m_dead_users.pop_back();
            }
        }

        if (timeout)
        {
            m_utils.timer_handler(m_timer_manager, m_users ,Webserver::m_user_count);
            LOG_INFO("timer tick");
            timeout = false;
        }    
    }
};

