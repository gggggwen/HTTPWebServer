#ifndef TIMING_WHEEL_H
#define TIMING_WHEEL_H
/*
   注意代码的可拓展性
   提供增删查改的接口  
   增:一般情况下是指针指向的前一个桶直接添加就完了
   删:指针指向的桶 , 删除之后指针立马前移 
   改: 在本操作中仅 , 可能会将转移桶内元素 
*/
#include"../log/log.h"
#include"../service/http_conn.h"
#include"timer.h"

typedef std::unordered_map<int , timer*> slot ;  

class timing_wheel
{
public:
   timing_wheel() 
   {
      m_maxinterval = 0 ;
      m_slot_num = 0 ;
      m_INTERVAL = 0 ; 
      m_tail = 0 ;

   }

   ~timing_wheel()
   {
      for(int i = 0 ; i<m_slot_num ; i++)
      {
          delete m_circular_queue[i] ; 
          m_circular_queue[i] = nullptr ; 
      }
   }

   void init(int timeslot, int slot_num  ) ;//延迟初始化

   timer* del_timer(int user_fd);

   void add_timer(timer* user_data );

   //连接中有新请求发送
   void adjust_timer(int user_fd);

   void tick(std::unordered_map<int, std::shared_ptr<http_connection>> &users, int &user_count, int &epfd);

private:
   int m_maxinterval; //超时时间 , 以秒计数
   int m_INTERVAL ;   //需要保证slot间的时等同于触发tick的时间
   int m_slot_num ; 
   int m_tail     ;
   std::vector<slot*> m_circular_queue;
};


#endif