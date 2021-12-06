//
// Created by ubuntu on 2021/12/2.
//

#ifndef MYWEBSERVER_LOCKER_H
#define MYWEBSERVER_LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

//信号量
class sem
{
public:
    sem()
    {   //始化一个定位在 sem 的匿名信号量
        //sem ：指向信号量对象
        //pshared : 指明信号量的类型。pshared 的值为 0，那么信号量将被进程内的线程共享
        // value : 指定信号量值的大小。
        if(sem_init(&m_sem,0,0)!=0) throw std::exception();
    }
    sem(int num)
    {
        if(sem_init(&m_sem,0,num)!=0) throw std::exception();
    }
    ~sem()
    {
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        return sem_wait(&m_sem)==0;
    }
    bool post()
    {
        return sem_post(&m_sem)==0;
    }

private:
    sem_t m_sem;
};

//互斥锁
class locker
{
public:
    locker()
    {
        //互斥锁的初始化
        //参数attr为NULL，则使用默认的互斥锁属性.
        //当一个线程加锁以后，其余请求锁的线程将形成一个等待队列，并在解锁后按优先级获得锁。这种锁策略保证了资源分配的公平性。
        if(pthread_mutex_init(&m_mutex,NULL)!=0) throw std::exception();
    }
    ~locker()
    {
        return pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex)==0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex)==0;
    }
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

//条件变量
class cond
{
public:
    cond()
    {
        //初始化一个条件变量
        //参数cattr为空指针等价于cattr中的属性为缺省属性
        if(pthread_cond_init(&m_cond,NULL)!=0) throw std::exception();
    }

    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    //等待资源，被阻塞在该条件变量的队列上，阻塞返回true,没阻塞返回false
    bool wait(pthread_mutex_t *m_mutex)
    {
        int ret=0;
        //将解锁mutex参数指向的互斥锁，并使当前线程阻塞在cv参数指向的条件变量上。
        //被阻塞的线程可以被pthread_cond_signal函数，pthread_cond_broadcast函数唤醒，也可能在被信号中断后被唤醒。
        //等待条件变量为真，成功返回0
        ret=pthread_cond_wait(&m_cond,m_mutex);
        return ret==0;
    }
    bool timewait(pthread_mutex_t *m_mutex,struct timespec t)
    {
        int ret=0;
        //阻塞直到指定时间
        //函数到了一定的时间，即使条件未发生也会解除阻塞。这个时间由参数abstime指定。
        //当在指定时间内有信号传过来时，pthread_cond_timedwait()返回0，否则返回一个非0数
        ret = pthread_cond_timedwait(&m_cond,m_mutex,&t);
        return ret==0;
    }
    bool signal()
    {
        //被用来释放被阻塞在指定条件变量上的一个线程
        return pthread_cond_signal(&m_cond)==0;
    }
    bool broadcast()
    {
        //释放阻塞的所有线程
        //唤醒所有被pthread_cond_wait函数阻塞在某个条件变量上的线程，参数cv被用来指定这个条件变量。
        // 当没有线程阻塞在这个条件变量上时，pthread_cond_broadcast函数无效。
        return pthread_cond_broadcast(&m_cond)==0;
    }

private:
    pthread_cond_t m_cond;
};
#endif //MYWEBSERVER_LOCKER_H
