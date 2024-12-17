#include "http_conn.h"

std::string http_connection::accept_language = "zh-CN,zh" ;
const std::string ok_200_title = "HTTP/1.1 200 OK";
const std::string error_400_title = "HTTP/1.1 400 Bad Request";
const std::string error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const std::string error_403_title = "HTTP/1.1 403 Forbidden";
const std::string error_403_form = "You do not have permission to get file from this server.\n";
const std::string error_404_title = "HTTP/1.1 404 Not Found";
const std::string error_404_form = "The requested file was not found on this server.\n";
const std::string error_500_title = "HTTP/1.1 500 Internal Error";
const std::string error_500_form = "There was an unusual problem serving the request file.\n";



void 
http_connection::clear_data()
{
    memset(read_buffer , 0 ,READ_BUFFER_SIZE) ; 
    memset(write_buffer , 0 ,WRITE_BUFFER_SIZE) ;
    read_offset = 0 ;
    write_offset = 0 ;

    m_action = UNDEFINED;
    m_method = UNKOWN;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_http_code = GET_REQUEST;

    file_path.clear() ; 
    passwd.clear() ;
    user.clear() ; 
}

void 
http_connection::init(int sockfd) 
{
    m_sockfd = sockfd ; 
    m_action = UNDEFINED;
    m_method = UNKOWN ;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_http_code = GET_REQUEST;
    close_conn = true  ; 

    write_offset = 0 ; 
    read_offset = 0 ; 
    read_buffer = new char[READ_BUFFER_SIZE] ; 
    memset(read_buffer , 0 , READ_BUFFER_SIZE) ;
    write_buffer = new char[WRITE_BUFFER_SIZE] ; 
    memset(write_buffer , 0 , WRITE_BUFFER_SIZE) ; 
};

http_connection::LINE_STATUS 
http_connection::parse_line()
{
    char *res = strpbrk(read_buffer + read_offset, "\r\n");
    if (!res || (*res == '\r' && res - read_buffer + 1 >= READ_BUFFER_SIZE))
    {
        LOG_DEBUG("LINE_OPEN");
        return LINE_OPEN;
    }
    else if (*res == '\n' || (*res == '\r' && *(res + 1) != '\n'))
    {
        LOG_DEBUG("LINE_BAD");
        return LINE_BAD;
    }
    return LINE_OK;
};

/*会根据权限以及文件是否存在 更新HTTP解码*/
bool 
http_connection::file_available()
{
    std::ifstream file(file_path);
    if (!file)
    {
        switch (errno)
        {
        case EACCES:
            LOG_INFO("permission was denied for this file %s in sockfd %d ", file_path.c_str(), m_sockfd);
            m_http_code = FORBIDDEN_REQUEST ;
            close_conn = true;
            break;
        case ENOENT:
            LOG_INFO("The file %s does not exist in sockfd %d ", file_path.c_str(), m_sockfd);
            m_http_code = NO_RESOURCE ;
            close_conn = true;
            break;
        default:
            LOG_ERROR("Unable to open file %s in sockfd %d" , file_path.c_str() , m_sockfd) ;
            m_http_code = INTERNAL_ERROR ;
            close_conn = true;
            break;
        }
    }
    return true ; 
};

/*获取具体的请求业务类型*/
std::string
get_path(char* s)
{
    int range = strchr(s , ' ') - s ; 
    return std::string(s , range) ; 
};

int
http_connection::get_file_size()
{
    struct stat info ; 
    stat(file_path.c_str() , &info) ; 
    return info.st_size ; 
};

std::string 
http_connection::get_file_path()
{
    char *tmp = read_buffer + read_offset;
    std::string s1 = "filepath=";
    char *res = strstr(tmp, s1.c_str()); // 检测是否有filepath字段
    if (!res)
    {
        LOG_ERROR("usr field can not be found in content in sockfd %d", m_sockfd);
        return std::string("F");
    }
    res = res + s1.size();   // 更新res到filepath值首位
    return std::string(res); // 行尾无"\r\n"
}


bool
http_connection::get_user_passwd()
{
    char* tmp = read_buffer+read_offset ; 
    std::string s1 = "user=" ; 
    std::string s2 = "password=" ; 
    char* res = strstr(tmp , s1.c_str()) ; //检测是否有"user字段 
    if(!res)
    {
        LOG_ERROR("usr field can not be found in content in sockfd %d", m_sockfd);
        return false ;
    }
    res = res+s1.size() ; //更新res到user值首位

    char *depart = strchr(tmp, '&'); 
    user = std::string(res , depart-res) ;
    tmp = depart+1 ; //更新tmp 方便比较字符串
    res = strstr(tmp, s2.c_str()) ;  // 检测是否有"password"字段
    if (!res){
        LOG_ERROR("passwd field can not be found in content in sockfd %d", m_sockfd);
        return false;
    }
    res = res+s2.size() ;  //更新res到password值首位
    passwd = res ; //行尾无"\r\n"
    return true ; 
};


bool 
http_connection::mysql_process()
{
    MYSQL_ROW  row ; 
    MYSQL_RES *res;
    sql_connection RAII_conn ;
    char query[256];
    std::string user_table = "user" ;
    MYSQL *mysql_conn = RAII_conn.get_connection();

    if (m_action == REGISTER)
    {
        /*先判断是否存在该用户 若没有插入表中,  若存在则判断密码是否匹配*/
        snprintf(query, sizeof(query), "SELECT passwd FROM passwd WHERE user = '%s'", user.c_str());
        if (mysql_query(mysql_conn, query)){
            LOG_ERROR( "Query failed: %s", mysql_error(mysql_conn));
            return false ;
        }

        res = mysql_store_result(mysql_conn);
        if (!res){
            LOG_ERROR("Failed to retrieve result: %s", mysql_error(mysql_conn))
            return false ;
        }

        if (mysql_num_rows(res) == 0){
            // 用户不存在，插入新用户
            mysql_free_result(res);
            snprintf(query, sizeof(query), "INSERT INTO passwd (user, passwd) VALUES ('%s', '%s')", user.c_str(), passwd.c_str());
            if (mysql_query(mysql_conn, query)){
                LOG_ERROR("Insert failed: %s", mysql_error(mysql_conn));
                return false ; 
            }
            else{
                /*插入成功*/
                LOG_INFO("User '%s' added successfully.", user.c_str());
                file_path = "../src/log.html";
            }
        }
        else{
            LOG_INFO("User %s already exist!" , user.c_str());
            file_path = "../src/registerError.html";
            mysql_free_result(res);
        }
    }
    else if(m_action == LOGIN)
    {
        /*先判断是否存在该用户 若没有插入表中,  若存在则判断密码是否匹配*/
        snprintf(query, sizeof(query), "SELECT passwd FROM passwd WHERE user = '%s'", user.c_str());
        if (mysql_query(mysql_conn, query))
        {
            LOG_ERROR("Query failed: %s", mysql_error(mysql_conn));
            return false;
        }

        res = mysql_store_result(mysql_conn);
        if (!res)
        {
            LOG_ERROR("Failed to retrieve result: %s", mysql_error(mysql_conn))
            return false;
        }

        if (mysql_num_rows(res) == 0)
        {
            // 用户不存在, 登录失败
            LOG_INFO("the user is not exist !") ;
            file_path = "../src/logError/html" ;
            mysql_free_result(res);
        }
        else
        {
            // 用户存在，验证密码
            row = mysql_fetch_row(res);
            if (strcmp(row[0], passwd.c_str()) == 0)
            {
                LOG_INFO("Password matches for user '%s'.", user.c_str());
                file_path = "../src/user_interface.html";
            }
            else
            {
                LOG_INFO("Password does not match for user '%s'", user.c_str());
                file_path = "../src/logError.html";
            }
            mysql_free_result(res);
        }
    }
    return true ; 
}

/*解析报文的步骤*/
bool 
http_connection::analyze_get()
{
    //get 会存在两种可能
    char* tmp = read_buffer + read_offset+4  ; //4为偏移量'
    std::string path = get_path(tmp) ;

    if(path == "/")
    {
        file_path = "../src/judge.html" ;
    }
    else if (path=="/favicon.ico" )
    {
        file_path = "../src/favicon.ico" ; 
    }
    else 
    {
        LOG_ERROR("analyze get in request line error ");
        return false;
    }
    
    return true ;
};

bool 
http_connection::analyze_post()
{
    /*5为偏移量"POST "*/
    char* tmp = read_buffer+read_offset+5 ;
    std::string path = get_path(tmp) ;
    if(path =="/PR")  //注册, 会传来usr-passwd 
    {
        m_action = REGISTER ;
    }
    else if(path =="/PL") //登录 会传来usr-passwd
    {
        m_action = LOGIN ;
    }
    else if(path =="/IR") //请求注册界面
    {
        m_action = G_REGISTER_HTML ;
        file_path = "../src/register.html";
    }
    else if(path=="/IL") //请求登录界面
    {
        m_action = G_LOGIN_HTML ;
        file_path = "../src/log.html";
    }
    else if(path == "/GF") //请求文件资源
    {
        m_action = GET_FILE; 
    }
    else if (path == "/GD") // 请求文件资源
    {
        m_action = GET_DIR;
    }
    else 
    {
        LOG_ERROR("getting POST path error ouccrred") ; 
        return false  ; 
    }
    return true ; 
};

bool 
http_connection::analyze_del()
{
    /*7为偏移量"DELETE "*/
    char *tmp = read_buffer + read_offset + 7;
    std::string path = get_path(tmp);
    if (path.size()<= 1 ) 
    {
        LOG_ERROR("getting DELETE file path error ouccrred") ; 
        return false ; 
    }
    else 
    {
        file_path = "../src"+path ; 
    }
    return true ; 
};


bool
http_connection::parse_request_line()
{
    char method[8] = {0};
    char* tmp = read_buffer+read_offset ; 
    size_t edge = strchr(tmp , ' ') -  tmp ;
    strncpy(method, tmp , edge);
    if (!strstr(tmp , "HTTP/1.1"))
    {
        LOG_ERROR("The http protocol version does not meet the requirements");
        return false;
    }

    if (strcmp(method, "GET") == 0)
    {
        m_method = GET;
        if(!analyze_get())
            return false ; 
        LOG_INFO("the sockfd %d method GET ", m_sockfd);
    }
    else if (strcmp(method, "POST") == 0)
    {
        // 用混可能会请求上传文件 , 或者发送用户-密码表单
        LOG_INFO("the sockfd %d method POST ", m_sockfd);
        if(!analyze_post())
            return false ; 
        m_method = POST;
    }
    else if (strcmp(method, "DELETE") == 0)
    {
        // 用户请求删除文件
        LOG_INFO("the sockfd %d method DELETE ", m_sockfd);
        if(!analyze_del())
            return false ; 
        m_method = DELETE;
    }
    else
    {
        m_method = UNKOWN;
        LOG_ERROR("unkown http method occured !")
        return false;
    }
    /*刷新read_offset*/
    read_offset = strpbrk(tmp, "\r\n") - read_buffer + 2;
    return true;
}

bool 
http_connection::parse_request_header()
{
    /*解析connection 字段和 Accept-Language 字段 */
    char* tmp = read_buffer + read_offset ; 
    std::string s1 = "Connection: " ; 
    std::string s2 = "Accept-Language: " ;

    char *offset_tmp = strchr(tmp, ' '); // 匹配
    if (!offset_tmp)
    {
        LOG_ERROR(" field in HTTP header can not be found ");
        return false;
    }
    offset_tmp++ ;  //为了使offset_tmp 首位正好在value上
    int range = offset_tmp - tmp ;
    std::string key(tmp , range) ; 
    if(key== s1)
    {
        if (strncmp(offset_tmp , "keep-alive", 10) == 0)
        {
            close_conn = false ;
            LOG_INFO("the sockfd %d connection state KEEP-ALIVE ", m_sockfd);
        }
        else if (strncmp(offset_tmp , "close", 5) == 0)
        {
            close_conn = true;
            LOG_INFO("the sockfd %d connection state CLOSE ", m_sockfd);
        }
        else
        {
            LOG_ERROR("the field of request header is not complete");
            close_conn = true;
            return false;
        }
    }
    else if(key==s2)
    {
        if (strncmp(offset_tmp, http_connection::accept_language.c_str(), http_connection::accept_language.size()) != 0)
        {
            LOG_INFO("Accept-Language can not be matched correctly");
            return false;
        }
        /*s2为最后一个字段*/
        m_check_state = CHECK_STATE_CONTENT;
    }
    /*刷新read_offset*/
    read_offset = strpbrk(tmp ,"\r\n") - read_buffer +2;
    return true ; 
}

bool 
http_connection::parse_request_body()
{
    /*需要获取密码, 文件等信息*/
    if(m_method==GET)
    {
        m_check_state = DONE ; 
        return true ; 
    }

    switch (m_action)
    {
    case G_REGISTER_HTML :
        break;
    case G_LOGIN_HTML :
        break ; 
    case REGISTER :
        if(!get_user_passwd() )
        {
            return false ; 
        } 
        break ; 
    case LOGIN :
        if (!get_user_passwd())
        {
            return false;
        }
        break ; 
    case GET_DIR :

        break ; 
    case GET_FILE :
        file_path = get_file_path();
        if (file_path.size() == 1 && file_path == "F")
        {
            file_path.clear();
            return false;
        }
        file_available() ;
        break; 
    case DELETE_FILE:
        /*待定*/
        break ; 
    default:
        return false  ; 
        break;
    }
    m_check_state = DONE;
    return true;
}


http_connection::HTTP_CODE 
http_connection::process_read()
{
    LINE_STATUS line_stat = LINE_OK ;

    while( ( ( line_stat = parse_line()) ==LINE_OK) && m_check_state!= DONE )
    {
        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
                if (!parse_request_line())
                {
                    LOG_ERROR("request line error occurred ");
                    if (m_method == UNKOWN)
                        return METHOD_NOT_ALLOWED;
                    return BAD_REQUEST;
                }
                m_check_state = CHECK_STATE_HEADER;
                break;
            case CHECK_STATE_HEADER:
                if (!parse_request_header())
                {
                    LOG_ERROR("request header  error occurred ");
                    return BAD_REQUEST;
                }
                // 内部会进行状态转换
                break;
            case CHECK_STATE_CONTENT:
                if (!parse_request_body())
                {
                    LOG_ERROR("request body error occurred");
                    return BAD_REQUEST;
                }
                break;
            default:
                LOG_ERROR("when parse request datagram unkown error occurred ");
                return BAD_REQUEST;
                break;
        }
    }
    if(line_stat == LINE_BAD )
        return BAD_REQUEST ; 
    else if(line_stat == LINE_OPEN)
        return NO_REQUEST ;

    return GET_REQUEST ; 
}

bool
http_connection::do_request()
{
    /*ile_available()  会自动修改http_code*/
    if (m_method == GET)
    {
        file_available() ; 
        return true ; 
    }

    switch (m_action)
    {
    case G_REGISTER_HTML:
        file_available() ;
        break;
    case G_LOGIN_HTML :
        file_available();
        break;
    case REGISTER:
        if(!mysql_process()) 
        {
            m_http_code = INTERNAL_ERROR; // 我的要求是必须给前端响应
            return true ; 
        }
        file_available();
        break;
    case LOGIN:
        if (!mysql_process())
        {
            m_http_code = INTERNAL_ERROR; // 我的要求是必须给前端响应
            return true ; 
        }
        file_available();
        break;
    case GET_FILE:
        break;
    case DELETE_FILE:
        break;
    default:
        LOG_ERROR("undefined action occurred ") ; 
        return false  ; 
        break;
    }
    if(file_path.size()> 0 )
    {
        LOG_INFO("the sockfd %d response file %s", m_sockfd, file_path.c_str());
    }
    return true  ; 
}

/*生成响应报文的步骤*/
void 
http_connection::generate_response_line()
{
    std::string line ; 
    switch (m_http_code)
    {
    case GET_REQUEST:
        line = ok_200_title + "\r\n" ; 
        break;
    case NO_RESOURCE:
        line = error_404_title + "\r\n" ;
        break;
    case FORBIDDEN_REQUEST:
        line = error_403_title + "\r\n";
        break;
    case INTERNAL_ERROR:
        line = error_500_title + "\r\n";
        break;
    default:
        LOG_ERROR("ilegal http_code occurred ");
        return ; 
        break;
    }
    memcpy(write_buffer+write_offset, line.c_str(), line.size());
    write_offset +=line.size() ; 
}

void 
http_connection::generate_response_header()
{
    std::string str ; 
    int len ; 
    switch (m_http_code)
    {
    case GET_REQUEST:
         len = get_file_size() ; 
         str = "Content-Length: "+std::to_string(len)+"\r\n" ; 
         break;
     case NO_RESOURCE:
         len = error_404_form.size() ;
         str = "Content-Length: " + std::to_string(len) + "\r\n";
         break;
     case FORBIDDEN_REQUEST:
         len = error_403_form.size();
         str = "Content-Length: " + std::to_string(len) + "\r\n";
         break;
     case INTERNAL_ERROR:
         len = error_500_form.size();
         str = "Content-Length: " + std::to_string(len) + "\r\n";
         break;
     default:
         LOG_ERROR("ilegal http_code occurred ");
         return ; 
         break;
     }

     memcpy(write_buffer+write_offset , str.c_str() ,  str.length()) ; 
     write_offset+=str.length() ;
     if(close_conn == true){
        str= "Connection: close\r\n" ; 
    } 
     else{
        str = "Connection: keep-alive\r\n" ; 
     }
     memcpy(write_buffer+write_offset , str.c_str() , str.length() ) ;
     write_offset +=str.length() ; 
     return ; 
}

void
http_connection::generate_response_body()
{
    memcpy(write_buffer+write_offset , "\r\n" , 2) ; 
    write_offset += 2 ; 
    int sz = 0 ;
    switch (m_http_code)
    {
    case GET_REQUEST:
        break;
    case NO_RESOURCE:
        sz = error_400_form.size() ; 
        memcpy(write_buffer+write_offset , error_404_form.c_str(), sz) ; 
        file_path.clear();
        break;
    case FORBIDDEN_REQUEST:
        sz = error_403_form.size() ; 
        memcpy(write_buffer + write_offset, error_403_form.c_str(), sz);
        file_path.clear();
        break;
    case INTERNAL_ERROR:
        sz = error_500_form.size() ; 
        memcpy(write_buffer + write_offset, error_500_form.c_str(), sz);
        file_path.clear();
        break;
    default:
        LOG_ERROR("ilegal http_code occurred ");
        break;
    }
    write_offset+=sz ;
    return ; 
}

bool
http_connection::process_write()
{
    generate_response_line() ; 
    generate_response_header() ; 
    generate_response_body() ; 
    if(write_offset == 0)
    {
        LOG_ERROR("proces write error occurred") ; 
        return false ;
    }
    return true ; 
}

bool 
http_connection::process()
{
    m_http_code = process_read() ; 
    if(m_http_code == BAD_REQUEST || m_http_code == NO_REQUEST ) 
    {
        return false ;     
    }

    bool res = do_request() ; 
    if(m_http_code == BAD_REQUEST || !res)
    {
        return false ; 
    }

    res = process_write() ; 
    if(!res)
    {
        return false  ; 
    }    
    return true ; 
}

