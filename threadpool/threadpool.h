//
// Created by ubuntu on 2021/12/2.
//

#ifndef MYWEBSERVER_THREADPOOL_H
#define MYWEBSERVER_THREADPOOL_H
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
    threadpool(int actor_model,connection_pool *connectionPool,int thread_number=8,int max_request=10000);
    ~threadpool();
    bool append(T *request,int state);
    bool append_p(T *request);
private:
    static void *worker(void *arg);
    void run();
private:
    int m_thread_number;//线程池中的线程数
    int m_max_requests;//请求队列中允许的最大请求数
    pthread_t *m_threads;//描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue;//请求队列
    locker m_queuelocker;//保护请求队列的互斥锁
    sem m_queuestat;//是否有任务需要处理
    connection_pool *m_connPool;//数据库
    int m_actor_model;//模型切换
};

template <typename T>
threadpool<T>::threadpool<typename T>(int actor_model, connection_pool *connectionPool, int thread_number,
                                      int max_request) :m_actor_model(actor_model), m_thread_number(thread_number), m_max_requests(max_request),
                                                        m_threads(NULL), m_connPool(connPool){
    if(thread_number<=0||max_request<=0) throw std::exception();
    m_threads=new pthread_t[m_thread_number];
    if(!m_threads) throw std::exception();

    for(int i=0;i<thread_number;++i)
    {
        //如果有一个工作线程创造失败，全部删除
        if(pthread_create(m_threads+i,NULL,worker,this)!=0)
        {
            delete [] m_threads;
            throw std::exception();
        }
        //如果有一个线程分离失败，全部删除
        if(pthread_detach(m_threads[i]))
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
                                      }

template <typename T>
threadpool<T>::~threadpool() {
        delete[] m_threads;
}

//添加到队列
template <typename T>
bool threadpool<T>::append(T *request, int state) {
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    request->m_state=state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}
template <typename T>
bool threadpool<T>::append_p(T *request) {
    m_queuelocker.lock();
    if(m_workqueue.size()>=m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

//工作线程
template <typename T>
void *threadpool<T>::worker(void *arg) {
    threadpool *pool=(threadpool *)arg;
    pool->run();
    return pool;
}
//取出一个工作线程
template <typename T>
void threadpool<T>::run()
{
    while(true)
    {
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        //从队首取出
        T *request = m_workqueue.front();
        m_queuelocker.unlock();
        if(!request) continue;
        //模型
        if(m_actor_model==1)
        {
            if(request->m_state==0)
            {
                if(request->read_once())
                {
                    request->improv=1;
                    //从连接池中取出一个数据库连接
                    connectionRAII mysqlcon(&request->mysql,m_connPool1);
                    request->process();
                }
                else
                {
                    request->improv=1;
                    request->timer_flag=1;
                }
            }
            else
            {
                if(request->write()) request->improv=1;
                else
                {
                    request->improv=1;
                    request->timer_flag=1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql,m_connPool);
            request->process();
        }
    }
}
#endif //MYWEBSERVER_THREADPOOL_H
