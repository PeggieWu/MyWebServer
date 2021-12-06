//
// Created by ubuntu on 2021/12/2.
//
/*
 * 循环数组实现的阻塞队列 m_back = (m_back+1)%m_max_size;
 * 线程安全：每个操作前都要先加互斥锁，操作完 再解锁
 * */
#ifndef MYWEBSERVER_BLOCK_QUEUE_H
#define MYWEBSERVER_BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"
using namespace std;

//阻塞队列
//项目中需要队列的有
//工作线程、消息队列
template <class T>
class block_queue
{
public:

    block_queue(int max_size=1000)
    {
        if(max_size<=0) exit(-1);

        m_max_size=max_size;
        m_array = new T[max_size];
        m_size=0;
        m_front=-1;
        m_back=-1;
    }

    void clear()
    {
        m_mutex.lock();
        m_size=0;
        m_front=-1;
        m_back=-1;
        m_mutex.unlock();
    }

    ~block_queue()
    {
        m_mutex.lock();
        if(m_array!= nullptr) delete [] m_array;

        m_mutex.unlock();
    }
    //判断队列是否满
    bool full()
    {
        m_mutex.lock();
        if(m_size>=m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    //判断队列是否空
    bool empty()
    {
        m_mutex.lock();
        if(m_size==0)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    //返回队首元素
    bool front(T &value)
    {
        m_mutex.lock();
        if(m_size==0)
        {
            m_mutex.unlock();
            return false;
        }
        value=m_array[m_front];
        m_mutex.unlock();
        return true;
    }
    //返回队尾元素
    bool back(T &value)
    {
        m_mutex.lock();
        if(m_size==0)
        {
            m_mutex.unlock();
            return false;
        }
        value=m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    //队列长度
    int size()
    {
        int tmp=0;

        m_mutex.lock();
        tmp=m_size;

        m_mutex.unlock();
        return tmp;
    }

    //最大长度
    int max_size()
    {
        int tmp=0;

        m_mutex.lock();
        tmp=m_max_size;

        m_mutex.unlock();
        return tmp;
    }

    //往队列添加元素，需要将所有使用队列的线程先唤醒
    //当有元素push进队列，相当于生产者生产了一个元素
    //若当前没有线程等待条件变量，则唤醒无意义
    bool push(const T &item)
    {
        m_mutex.lock();
        if(m_size>=m_max_size)
        {
            m_cond.broadcast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back+1)%m_max_size;
        //放入队列后面
        m_array[m_back]=item;

        m_size++;
        //相当于使用了signal()
        m_cond.broadcast();
        m_mutex.unlock();
        return true;
    }

    //pop时，如果当前队列没有元素，将会等待条件变量
    bool pop(T &item)
    {
        m_mutex.lock();
        /*
             * 可能多个线程在等待这个资源可用的信号，信号发出后只有一个资源可用，
             * 但是有A，B两个线程都在等待，B比较速度快，获得互斥锁，然后加锁，消耗资源，然后解锁，
             * 之后A获得互斥锁，但他回去发现资源已经被使用了，它便有两个选择，一个是去访问不存在的资源，
             * 另一个就是继续等待，那么继续等待下去的条件就是使用while，要不然使用if的话pthread_cond_wait返回后，
             * 就会顺序执行下去。
             * */
        //该情况就是获得了互斥锁，但是没有资源可用，要继续等待
        while(m_size<=0)
        {
            //wait()一个资源，如果返回true说明成功了，等到了（m_size>0），false就是没成功
            //如果没等成功，退出，不等了
            if(!m_cond.wait(m_mutex.get()))
            {
                m_mutex.unlock();
                return false;
            }
        }
        //取出元素
        m_front = (m_front+1)%m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
    //增加了超时处理,函数到了一定的时间，即使条件未发生也会解除阻塞
    bool pop(T &item,int ms_timeout)
    {
        struct timespec t={0,0};
        struct timeval now={0,0};
        gettimeofday(&now,NULL);
        m_mutex.lock();
        if(m_size<=0)
        {
            t.tv_sec = now.tv_sec+ms_timeout/1000;
            t.tv_nsec=(ms_timeout%1000)*1000;
            if(!m_cond.timewait(m_mutex.get(),t))
            {
                m_mutex.unlock();
                return false;
            }
        }
        if(m_size<=0)
        {
            m_mutex.unlock();
            return false;
        }

        //取出元素
        m_front = (m_front+1)%m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;


    }
private:
    //该阻塞队列的互斥锁
    locker m_mutex;
    //该阻塞队列的条件变量
    cond m_cond;

    //队列
    T *m_array;
    //队列中拥有的元素数
    int m_size;
    //队列能存放的最大元素数
    int m_max_size;
    //队首
    int m_front;
    //队尾
    int m_back;
};
#endif //MYWEBSERVER_BLOCK_QUEUE_H
