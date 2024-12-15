cmake_minimum_required(VERSION 2.8.12.2) 

project(HttpServer)



set(SRC_LIST main.cc  webserver.cc  ./CGImysql/cgimysql.cc  ./lock/lock.h  ./log/log.cc  ./timer/timer.h ./timer/timing_wheel.cc  
      ./timer/timer_manager.cc ./threadpool/threadpool.h   ./service/http_conn.cc  config.cc  )


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")

set(EXECUTABLE_OUTPUT_PATH /home/gwen/github_repo/MyTinyWebServer )

add_executable(test ${SRC_LIST})

# 链接 pthread mysqlclient 库 
set(mysqlclient "/usr/lib64/mysql/libmysqlclient.so.18.1.0")
target_link_libraries(test pthread ${mysqlclient})

