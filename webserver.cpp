//
// Created by ubuntu on 2021/12/2.
//

#include "webserver.h"

WebServer::WebServer() {

    //http_conn类对象
    users=new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path,200);
    char root[6]="/root";
    m_root = (char*) malloc(strlen(server_path)+ strlen(root)+1);
    strcpy(m_root,server_path);
    strcat(m_root,root);

    //定时器
    user_timer = new client_data[MAX_FD];

}

WebServer::~WebServer() {

    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] user_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passWord, string databaseName, int log_write, int opt_linger,
                     int trigmode, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_passWord = passWord;
    m_databaseName = databaseName;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

void WebServer::trig_mode() {

    //LT+LT
    if(m_TRIGMode==0)
    {
        m_LISTENTrigmode=0;
        m_CONNTrigmode=0;
    }
    //LT+ET
    else if(m_TRIGMode==1)
    {
        m_LISTENTrigmode=0;
        m_CONNTrigmode=1;
    }
    //ET+LT
    else if(m_TRIGMode==2)
    {
        m_LISTENTrigmode=1;
        m_CONNTrigmode=0;
    }
    //ET+ET
    else if(m_TRIGMode==3)
    {
        m_LISTENTrigmode=1;
        m_CONNTrigmode =1;
    }
}

void WebServer::log_write() {

    if(m_close_log==0)
    {
        //初始化日志
        if(1==m_log_write)
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,800);
        else
            Log::get_instance()->init("./ServerLog",m_close_log,2000,800000,0);
    }
}

void WebServer::sql_pool() {

    //初始化数据库连接池
    m_connPool = connection_pool::GetInstance();
    m_connPool->init("localhost",m_user,m_passWord,m_databaseName,3306,m_sql_num,m_close_log);

    //初始化数据库读取表
    users->inint_mysql_result(m_connPool);
}

void WebServer::thread_pool() {

    //线程池
    m_pool=new threadpool<http_conn>(m_actormodel,m_connPool,m_thread_num);
}

void WebServer::eventListen() {

    //创建socket
    m_listenfd = socket(PF_INET,SOCK_STREAM,0);
    assert(m_listenfd>=0);

    //优雅关闭连接
    if(m_OPT_LINGER==0)
    {
        struct linger tmp={0,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof (tmp));
    }
    else if(m_OPT_LINGER==1)
    {
        struct linger tmp={1,1};
        setsockopt(m_listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof (tmp));
    }

    //创建ipv4地址
    int ret = 0;
    struct sockaddr_in address;
    bzero(&address,sizeof (address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag=1;
    setsockopt(m_listenfd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof (flag));
    //命名socket
    ret = bind(m_listenfd,(struct sockaddr *)&address,sizeof (address));
    assert(ret>=0);
    ret = listen(m_listenfd,5);
    assert(ret>=0);

    utils.addfd(m_epollfd,m_listenfd, false,m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX,SOCK_STREAM,0,m_pipefd);
    assert(ret!=-1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd,m_pipefd[0], false,0);

    utils.addsig(SIGPIPE,SIG_IGN);
    utils.addsig(SIGALRM,utils.sig_handler, false);
    utils.addsig(SIGTERM,utils.sig_handler, false);

    alarm(TIMESLOT);

    Utils::u_pipefd=m_pipefd;
    Utils::u_epollfd = m_epollfd;

}

void WebServer::timer(int connfd, struct sockaddr_in client_address) {

    users[connfd].init(connfd,client_address,m_root,m_CONNTrigmode,m_close_log,m_user,m_passWord,m_databaseName);

    user_timer[connfd].address = client_address;
    user_timer[connfd].sockfd = connfd;
    util_timer *timer = &user_timer[connfd];
}