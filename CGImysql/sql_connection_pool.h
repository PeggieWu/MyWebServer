//
// Created by ubuntu on 2021/12/2.
//

#ifndef MYWEBSERVER_SQL_CONNECTION_POOL_H
#define MYWEBSERVER_SQL_CONNECTION_POOL_H
#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

class connection_pool
{
public:
    MYSQL *GetConnection();

    bool ReleaseConnection(MYSQL *conn);

    int ReleaseConnection(MYSQL *conn);

    int GetFreeConn();

    void DestroyPool();

    static connection_pool* GetInstance();

    void init(string url,string User,String PassWord,string DataBaseName,int Port,int MaxConn,int close_log);

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;//最大连接数
    int m_CurConn;//当前已使用的连接数
    int m_FreeConn;//当前空闲的连接数
    locker lock;
    list<MYSQL *> connList;//连接池
    sem reserve;

public:
    string m_url;//主机地址
    string m_Port;//数据库端口号
    string m_User;//登陆数据库用户名
    string m_PassWord;//登陆数据库密码
    string m_DatabaseName;//使用数据库名
    int m_close_log;//日志开关

};

//连接池的创建回收机制
class connectionRAII{
public:
    connectionRAII(MYSQL **con,connection_pool *connPool);
    ~connectionRAII();

private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};
#endif //MYWEBSERVER_SQL_CONNECTION_POOL_H
