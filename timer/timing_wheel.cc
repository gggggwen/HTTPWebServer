#include"timing_wheel.h"

void timing_wheel::init(int interval, int slot_num) // 延迟初始化
{
    if (interval == 0 || slot_num == 0)
    {
        LOG_ERROR("时间槽 或者 桶数传入参数有误");
        return;
    }
    m_maxinterval = slot_num * interval;
    m_slot_num = slot_num;
    m_INTERVAL = interval;
    m_circular_queue.resize(m_slot_num , nullptr) ; 
    for (int i = 0; i < m_slot_num; i++)
    {
        m_circular_queue[i] = new slot;
    }
}

/*删除某个指定的计时器*/
timer* timing_wheel::del_timer(int user_fd)
{
    for (int i = 0; i < m_slot_num; i++)
    {
        if ((*m_circular_queue[i]).find(user_fd) != (*m_circular_queue[i]).end())
        {
            timer *temp = (*m_circular_queue[i])[user_fd];
            (*m_circular_queue[i]).erase(user_fd);
            return temp ;
        }
    }
    return nullptr  ;
}

void timing_wheel::add_timer(timer *user_data)
{
    if(!user_data) 
    {
        LOG_WARN("在添加计时器时 ,传入空的用户数据指针") ; 
    }
    int index = (m_tail + 1) % m_slot_num;
    LOG_INFO("Timer of sockfd:%d will be inserted into the %dth slot " , user_data->sockfd,  index) ; 
    (*m_circular_queue[index]).insert({user_data->sockfd, user_data});
    return;
}

/*某套接字有新请求传来 需要更新其位置*/
void timing_wheel::adjust_timer(int user_fd)
{
    for (int i = 0; i < m_slot_num; i++)
    {
        if ((*m_circular_queue[i]).find(user_fd) != (*m_circular_queue[i]).end())
        {
            timer *temp = (*m_circular_queue[i])[user_fd];
            if(!temp) continue ; 
            (*m_circular_queue[i]).erase(user_fd);

            add_timer(temp);
            break;
        }
    }
    return;
}

void timing_wheel::tick(std::unordered_map<int, std::shared_ptr<http_connection>> &users , int& user_count, int& epfd)
{
    if((*m_circular_queue[m_tail]).empty()) 
    {
        m_tail =  m_tail==0 ? m_slot_num -1 :  (m_tail - 1) % m_slot_num;
        return ; 
    } 
    for (auto it = (*m_circular_queue[m_tail]).begin(); it != (*m_circular_queue[m_tail]).end();)
    {
        timer *temp = it->second;
        if (!users[temp->sockfd].unique())
        {    
            continue ; 
        }

        /*1.从epoll内核事件表注销 2.关闭套接字 3.用户数修改 4.注销用户类*/
        users.erase(temp->sockfd)  ; //删除元素 , std::shared_ptr会自动释放资源
        epoll_ctl(epfd, EPOLL_CTL_DEL, temp->sockfd, NULL);
        close(temp->sockfd)  ;
        user_count --  ;

        it = (*m_circular_queue[m_tail]).erase(it); // 防止迭代器失效
        delete temp;
        LOG_ERROR("有套接字连接已超时, 强制关闭") ;
    }
    /*改变指针*/
    m_tail = m_tail == 0 ? m_slot_num - 1 : (m_tail - 1) % m_slot_num;
    return;
}
