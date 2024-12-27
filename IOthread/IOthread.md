# Class O_thread

**为了降低主线程负载, 因此将输出分离, 由单独的线程进行处理**

其设计非常类似于线程池 , 同样提供如下两个列表:

```c++
std::list<std::shared_ptr<T>> m_users_queue; //类似于线程池的 m_jov_queue
std::list<int> m_dead_users;  
```



## 1.如何处理写事件

首先把将write_buffer数据拷贝到套接字上, 这个不作过多赘述。

### 	1.1文件传输

获取文件大小由http_connection类中函数实现

**分为两种情况:** 

- 大于1024*10字节

采用内存映射进行传输, 能够减少拷贝次数, 提生效率

注意:内存映射提供的指针不需要手动分配内存, `mmap`会自动分配, 且也不需要手动释放 调用`ummap`会自动释放掉内存空间

- 小于1024*10字节

传统IO函数进行传输



###     1.2清空http_connection user

为了提升程序运行效率, 每次完成一个任务处理周期, 并不会马上销毁该用户实例, 而是会暂存在内存中, 为了保证下一次数据不会在buffer中出现混乱的情况。所以需要清空所有有关本次任务的数据

```c++
/*完成数据传输后, 清空所有有关本次任务的数据*/
user->clear_data();

/*在http_conn.cc中*/
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

    dir_path.clear() ; 
    file_path.clear() ; 
    passwd.clear() ;
    user.clear() ; 
}
```



## 2.异常处理

如果在传输数据时, **出现任何异常都将立即停止对该用户进行数据传输**并将该用户对应的套接字连接追加至`m_dead_users`中让主线程来处理
