cmake_minimum_required(VERSION 2.8.12.2) 

project(HttpServer)



set(SRC_LIST main.cc  webserver.cc  ./CGImysql/cgimysql.cc  ./ipc/lock.h  ./ipc/fifo.h ./log/log.cc  ./timer/timer.h ./timer/timing_wheel.cc  
      ./timer/timer_manager.cc ./threadpool/threadpool.h  ./IOthread/IO_thread.h ./service/http_conn.cc  ./config/config.cc  )


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")

#可自行調整
set(EXECUTABLE_OUTPUT_PATH /home/gwen/github_repo/HTTPWebServer )

add_executable(server ${SRC_LIST})

# 链接 pthread mysqlclient 库 
set(mysqlclient "/usr/lib64/mysql/libmysqlclient.so.18.1.0")
target_link_libraries(server pthread ${mysqlclient})

