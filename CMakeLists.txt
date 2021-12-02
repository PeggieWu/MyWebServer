cmake_minimum_required(VERSION 3.20)
project(MyWebServer)

set(CMAKE_CXX_STANDARD 11)

add_executable(MyWebServer main.cpp config.h webserver.h webserver.cpp config.cpp threadpool/threadpool.h http/http_conn.h CGImysql/sql_connection_pool.h lock/locker.h log/log.h)
