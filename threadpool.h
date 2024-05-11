#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"
#include "message.h"
#include "raft_server.h"


class SocketTask {
public:
    int socket_fd;          // 套接字文件描述符
    Message message;       // 从套接字读取的消息

    SocketTask(int fd, const Message& msg) : socket_fd(fd), message(msg) {}
};

class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    raft_server* rs;
    threadpool(int actor_model, int thread_number = 8, int max_request = 10000);
    threadpool();
    ~threadpool();
    bool append(SocketTask* request);

private:

    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<SocketTask*> m_workqueue; //请求队列
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //是否有任务需要处理
    int m_actor_model;          //模型切换
};

threadpool::threadpool( int actor_model,  int thread_number, int max_requests) : m_actor_model(actor_model),m_thread_number(thread_number), m_max_requests(max_requests)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    for (int i = 0; i < thread_number; ++i)
    {
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
threadpool::~threadpool()
{
    delete[] m_threads;
}

bool threadpool::append(SocketTask* s)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(s);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}


void *threadpool::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

void threadpool::run() {
    while (true) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        SocketTask *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if (!request) {
            continue;
        }
        
        // 根据消息类型调用相应的处理函数
        switch (request->message.type) {
            case MessageType::VoteRequest:
            void handle_vote_request(int fd,const std::string& message);//处理投票请求
                rs->handle_vote_request(request->socket_fd,request->message);//把message对象和来自哪个fd传入
                break;
            case MessageType::VoteResponse:
                rs->handle_vote_response(request->socket_fd,request->message);
                
                break;
            case MessageType::Heartbeat:
                rs->handle_heartbeat(request->socket_fd,request->message);
                break;
            case MessageType::AppendEntries:
                rs->handle_append_entries(request->socket_fd,request->message);
                break;
            case MessageType::AppendEntriesResponse:
                rs->handle_append_entries_response(*request);
                break;
            // 可以添加更多的case来处理其他消息类型
            default:
                // 未知消息类型，可以记录日志或者抛出异常
                break;
        }

        // 处理完毕后，如果需要，可以删除request或者放回某个池子中以供复用
        delete request;
    }
}
#endif
