#include"log.h"

//非线程安全 , 记得加锁
inline void
Log::swap_buffer(BufferPtr& buffer_a, BufferPtr& buffer_b) noexcept
{
    BufferPtr tmp = buffer_a ; 
    buffer_a = buffer_b ;
    buffer_b = tmp  ;
}

void Log::init(const char *directory, int is_async)
{
    m_directory  = directory ; 
    m_is_async  = is_async ; 

    currentBuffer_ = new Buffer;
    readyBuffer_   = new Buffer ; 
    backstageBuffer_ = new Buffer  ;
    memset((void *)currentBuffer_, sizeof(Buffer), 0);
    memset((void *)readyBuffer_, sizeof(Buffer), 0);
    memset((void *)backstageBuffer_, sizeof(Buffer), 0);

    // 获取当前时间
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // 转换为时间结构
    time_t now = tv.tv_sec;
    m_log_tm = localtime(&now);
    
    std::string now_year = std::to_string(m_log_tm->tm_year + 1900);
    std::string now_month = m_log_tm->tm_mon >= 8 ? std::to_string(m_log_tm->tm_mon+1) : '0' + std::to_string(m_log_tm->tm_mon+1);
    std::string now_day = m_log_tm->tm_mday >= 10 ? std::to_string(m_log_tm->tm_mday) : '0' + std::to_string(m_log_tm->tm_mday);
    m_filename = now_year + '_' + now_month + '_' + now_day + "_ServerLog";

    std::string file_full_name = m_directory + m_filename;
    m_log_file.open(file_full_name, std::ios::app);

    if(m_is_async == 1)
    {
        std::thread t(write_log_thread) ; 
        t.detach() ;
    }
    return ; 
}

    void Log::write_log(int log_level, const char *str, ...)
{

    // 获取当前时间
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // 转换为时间结构
    time_t now = tv.tv_sec;
    struct tm *tm_info = localtime(&now);

    // 创建时间戳字符串
    std::string log_line(64, '\0'); // 预先分配64 字节的空间
    strftime(&log_line[0], log_line.length(), "%Y-%m-%d %H:%M:%S", tm_info);
    log_line.resize(strlen(log_line.c_str())); // 移除多余的 '\0' 字符

    log_line += ("." + std::to_string(tv.tv_usec));
    switch (log_level)
    {
    case 0:
        log_line = log_line + " [debug] ";
        break;
    case 1:
        log_line += " [info] ";
        break;
    case 2:
        log_line += " [warning] ";
        break;
    case 3:
        log_line += " [error] ";
        break;
    default:
        break;
    }
    // 构建可变参数字符串
    va_list args;
    va_start(args, str);
    char log_str[512]; // 临时缓冲区，适当调整大小
    vsnprintf(log_str, sizeof(log_str), str, args);
    va_end(args);
    log_line = log_line + log_str + "\n";


    if (m_is_async == 1) // 异步日志
    {
        //先判断日期
        mutex_.lock() ;
        if (m_log_tm->tm_mday < tm_info->tm_mday || (tm_info->tm_mday == 1 && m_log_tm->tm_mon < tm_info->tm_mon)) //若当前时间为最新日期才符合条件
        {
            swap_buffer(currentBuffer_ , readyBuffer_ ) ;
            if (currentBuffer_->ptr != 0)
            {
                memset((void *)currentBuffer_, 0, sizeof(Buffer)); // 交换后强制清空
            }

            m_log_file.close();
            *m_log_tm= *tm_info;    // 日期更新为最新日期
            std::string now_year = std::to_string(m_log_tm->tm_year + 1900);
            std::string now_month = m_log_tm->tm_mon >= 9 ? std::to_string(m_log_tm->tm_mon) : '0' + std::to_string(m_log_tm->tm_mon);
            std::string now_day = m_log_tm->tm_mday >= 10 ? std::to_string(m_log_tm->tm_mday) : '0' + std::to_string(m_log_tm->tm_mday);
            m_filename = now_year + '_' + now_month + '_' + now_day + "_ServerLog";

            
            std::string file_full_name = m_directory.back()=='/'? m_directory + m_filename : m_directory +'/'+m_filename;
            m_log_file.open(file_full_name, std::ios::app);

            cond_.notify_one();
            mutex_.unlock();        
        }
        else 
        {
            mutex_.unlock() ; 
        }
        
        mutex_.lock() ;
        if(readyBuffer_->ptr == 0 && currentBuffer_->ptr >256 )
        {
            swap_buffer(currentBuffer_ , readyBuffer_) ;
        }
        else if( currentBuffer_->ptr + log_line.length()>= BUFFERSIZE -1)
        {            
            swap_buffer(currentBuffer_ , readyBuffer_) ; //交换buffer
            if(currentBuffer_->ptr !=0)
            {
                memset((void *)currentBuffer_, 0, sizeof(Buffer));  //交换后强制清空
            }
        }
        std::strcat(currentBuffer_->buffer + currentBuffer_->ptr, log_line.c_str());
        currentBuffer_->ptr += log_line.length();
        currentBuffer_->buffer[currentBuffer_->ptr] = '\0';
        cond_.notify_one();
        mutex_.unlock();
        return ; 
    }
    else //同步日志
    {
        mutex_.lock() ;
        if (m_log_tm->tm_mday < tm_info->tm_mday || (tm_info->tm_mday ==1 && m_log_tm->tm_mon < tm_info->tm_mon) ) // 日期不一致需要更改 , 注意需要进行唤醒 7 月  8 月
        {
            
            *m_log_tm = *tm_info;
            std::string now_year = std::to_string(m_log_tm->tm_year + 1900);
            std::string now_month = m_log_tm->tm_mon >= 9 ? std::to_string(m_log_tm->tm_mon) : '0' + std::to_string(m_log_tm->tm_mon);
            std::string now_day = m_log_tm->tm_mday >= 10 ? std::to_string(m_log_tm->tm_mday) : '0' + std::to_string(m_log_tm->tm_mday);
            m_filename = now_year + '_' + now_month + '_' + now_day + "_ServerLog";
            std::string file_full_name = m_directory + m_filename;
            m_log_file.close(); 
            m_log_file.open(file_full_name, std::ios::app|std::ios::binary);
        }
        m_log_file<<log_line; 
        m_log_file.flush() ; 
        mutex_.unlock() ; 
        return ; 
    }
}


void
Log::async_write_log()
{
    // 初始化计时器
    struct timespec t = {0} ;
    t.tv_sec = 3 ;   //等待时间3s

    while(true)
    {
        if(backstageBuffer_->buffer[0]=='\0') //无数据可写
        {
            MutexLockguard lockguard(mutex_);
            cond_.timewait(mutex_ , t) ;   
            swap_buffer(readyBuffer_ , backstageBuffer_) ;
        }
        
        if(backstageBuffer_->buffer[0]!='\0') //如果交换后 buffer内有日志消息
        {
            m_log_file<< backstageBuffer_->buffer ;
            backstageBuffer_->buffer[0] = '\0' ;
            m_log_file.flush();
            backstageBuffer_->ptr = 0 ;
        }

        {
            MutexLockguard lockguard(mutex_) ; 
            swap_buffer(readyBuffer_ , backstageBuffer_ ); 
        }
    }
};