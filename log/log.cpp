//
// Created by ubuntu on 2021/12/5.
//

#include <cstring>
#include <ctime>
#include <ctime>
#include <cstdarg>

#include "log.h"
#include <pthread.h>
using namespace std;

Log::Log()
{
    m_count=0;
    m_is_async = false;
}

Log::~Log()
{
    if(m_fp!=nullptr)
        fclose(m_fp);
}
//异步需要设置阻塞队列的长度，同步不需要设置
bool Log::init(const char *file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //如果设置了max_queue_size，则设置为异步
    if(max_queue_size>=1)
    {
        m_is_async =true;
        //生成队列
        m_log_queue= new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数，这里表示创建线程异步写日志
        pthread_create(&tid,nullptr,flush_log_thread,nullptr);
    }

    m_close_log =close_log;
    m_log_buf_size=log_buf_size;
    //日志缓冲区
    m_buf = new char[m_log_buf_size];
    memset(m_buf,'\0',m_log_buf_size);
    m_split_lines=split_lines;

    time_t t= time(nullptr);
    struct tm *sys_tm= localtime(&t);
    struct tm my_tm = *sys_tm;

    //char *strrchr(const char *str, int c) 在参数 str 所指向的字符串中搜索最后一次出现字符 c（一个无符号字符）的位置。
    const char *p= strrchr(file_name,'/');
    char log_full_name[256]={0};
    //确定存储路径
    //存在当前目录下
    if(p==nullptr)
    {
        snprintf(log_full_name,255,"%d_%02d_%02d_%s",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,file_name);
    }
    //指定路径
    else
    {
        strcpy(log_name,p+1);
        strncpy(dir_name,file_name,p-file_name+1);
        snprintf(log_full_name,255,"%s%d_%02d_%02d_%s",dir_name,my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,log_name);
    }

    m_today =my_tm.tm_mday;
    //打开新日志文件
    m_fp=fopen(log_full_name,"a");
    if(m_fp==nullptr) return false;

    return true;

}
//写日志
void Log::write_log(int level, const char *format, ...) {
    struct timeval now={0,0};
    gettimeofday(&now,nullptr);
    time_t t=now.tv_sec;
    struct tm *sys_tm= localtime(&t);
    struct tm my_tm =*sys_tm;
    char s[16]={0};

    switch (level) {
        case 0:
            strcpy(s,"[debug]:");
            break;
        case 1:
            strcpy(s,"[info]:");
            break;
        case 2:
            strcpy(s,"[warn]");
            break;
        case 3:
            strcpy(s,"[error]");
            break;
        default:
            strcpy(s,"[info]");
            break;
    }

    m_mutex.lock();
    m_count++;

    //一天一个日志，一个日志放不下，换新的日志
    if(m_today!=my_tm.tm_mday||m_count%m_split_lines==0)
    {
        char new_log[256]={0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16]={0};

        snprintf(tail,16,"%d_%02d_%02d_",my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday);

        if(m_today!=my_tm.tm_mday)
        {
            snprintf(new_log,255,"%s%s%s",dir_name,tail,log_name);
            m_today=my_tm.tm_mday;
            m_count=0;
        }
        //当天的新日志
        else
        {
            snprintf(new_log,255,"%s%s%s.%lld",dir_name,tail,log_name,m_count/m_split_lines);
        }
        m_fp=fopen(new_log,"a");
    }
    m_mutex.unlock();
    va_list valst;
    //C 库宏 void va_start(va_list ap, last_arg) 初始化 ap 变量，
    // 它与 va_arg 和 va_end 宏是一起使用的。last_arg 是最后一个传递给函数的已知的固定参数，即省略号之前的参数。
    va_start(valst,format);

    string log_str;
    m_mutex.lock();

    //写入的具体时间内容格式
    int n= snprintf(m_buf,48,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
                   my_tm.tm_year+1900,my_tm.tm_mon+1,my_tm.tm_mday,my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec,now.tv_usec,s);

    int m= vsnprintf(m_buf+n,m_log_buf_size-1,format,valst);
    m_buf[n+m]='\n';
    m_buf[n+m+1]='\0';
    log_str=m_buf;

    m_mutex.unlock();

    //放入队列
    if(m_is_async&&m_log_queue->full())
        m_log_queue->push(log_str);
    else
    {
        m_mutex.lock();
        //继续写入文件
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    //结束变量列表
    va_end(valst);

}

void Log::flush(void) {
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}

