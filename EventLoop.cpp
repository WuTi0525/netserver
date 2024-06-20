#include "EventLoop.h"

int createtimerfd(int sec=30)
{
    int tfd=timerfd_create(CLOCK_MONOTONIC,TFD_CLOEXEC|TFD_NONBLOCK);   // 创建timerfd。
    struct itimerspec timeout;                                // 定时时间的数据结构。
    memset(&timeout,0,sizeof(struct itimerspec));
    timeout.it_value.tv_sec = sec;                             // 定时时间，固定为5，方便测试。
    timeout.it_value.tv_nsec = 0;
    timerfd_settime(tfd,0,&timeout,0);
    return tfd;
}

// 在构造函数中创建Epoll对象ep_。
EventLoop::EventLoop(bool mainloop,int timetvl,int timeout):ep_(new Epoll),mainloop_(mainloop),
                   timetvl_(timetvl),timeout_(timeout),stop_(false),
                   wakeupfd_(eventfd(0,EFD_NONBLOCK)),wakechannel_(new Channel(this,wakeupfd_)),
                   timerfd_(createtimerfd(timeout_)),timerchannel_(new Channel(this,timerfd_))

{
    wakechannel_->setreadcallback(std::bind(&EventLoop::handlewakeup,this));
    wakechannel_->enablereading();

    timerchannel_->setreadcallback(std::bind(&EventLoop::handletimer,this));
    timerchannel_->enablereading();
}

EventLoop::~EventLoop()
{
}

// 运行事件循环。
void EventLoop::run()                      
{
    threadid_=syscall(SYS_gettid);    // 获取事件循环所在线程的id。

    while (stop_==false)        // 事件循环。
    {
       std::vector<Channel *> channels=ep_->loop(10*1000);         // 等待监视的fd有事件发生。

        // 如果channels为空，表示超时，回调TcpServer::epolltimeout()。
        if (channels.size()==0)
        {
            epolltimeoutcallback_(this);
        }
        else
        {
            for (auto &ch:channels)
            {
                ch->handleevent();        // 处理epoll_wait()返回的事件。
            }
        }
    }
}

 // 停止事件循环。
 void EventLoop::stop()                    
 {
    stop_=true;
    wakeup();       // 唤醒事件循环，如果没有这行代码，事件循环将在下次闹钟响时或epoll_wait()超时时才会停下来。
 }

// 把channel添加/更新到红黑树上，channel中有fd，也有需要监视的事件。
void EventLoop::updatechannel(Channel *ch)                        
{
    ep_->updatechannel(ch);
}

 // 从黑树上删除channel。
 void EventLoop::removechannel(Channel *ch)                       
 {
    ep_->removechannel(ch);
 }

// 设置epoll_wait()超时的回调函数。
void EventLoop::setepolltimeoutcallback(std::function<void(EventLoop*)> fn)  
{
    epolltimeoutcallback_=fn;
}

// 判断当前线程是否为事件循环线程。
bool EventLoop::isinloopthread()   
{
    return threadid_==syscall(SYS_gettid); 
}

 // 把任务添加到队列中。
 void EventLoop::queueinloop(std::function<void()> fn)
 {
    {
        std::lock_guard<std::mutex> gd(mutex_);           // 给任务队列加锁。
        taskqueue_.push(fn);                                            // 任务入队。
    }

    wakeup();        // 唤醒事件循环。
 }

// 用eventfd唤醒事件循环线程。
 void EventLoop::wakeup()
 {
    uint64_t val=1;
    write(wakeupfd_,&val,sizeof(val));
 }

 // 事件循环线程被eventfd唤醒后执行的函数。
 void EventLoop::handlewakeup()
 {
    uint64_t val;
    read(wakeupfd_,&val,sizeof(val));       // 从eventfd中读取出数据，如果不读取，eventfd的读事件会一直触发。

    std::function<void()> fn;

    std::lock_guard<std::mutex> gd(mutex_);           // 给任务队列加锁。

    // 执行队列中全部的发送任务。
    while (taskqueue_.size()>0)
    {
        fn=std::move(taskqueue_.front());    // 出队一个元素。
        taskqueue_.pop();                              
        fn();                                                    // 执行任务。
    }
 }

// 闹钟响时执行的函数。
void EventLoop::handletimer()                                                 
{
    // 重新计时。
    struct itimerspec timeout;                                // 定时时间的数据结构。
    memset(&timeout,0,sizeof(struct itimerspec));
    timeout.it_value.tv_sec = timetvl_;                             // 定时时间，固定为5，方便测试。
    timeout.it_value.tv_nsec = 0;
    timerfd_settime(timerfd_,0,&timeout,0);

    if (mainloop_)
    {
        // printf("主事件循环的闹钟时间到了。\n");
    }
    else
    {
        // printf("从事件循环的闹钟时间到了。\n"); 
        time_t now=time(0);         // 获取当前时间。
        for (auto aa:conns_)
        {
            if (aa.second->timeout(now,timeout_)) 
            {
                {
                    std::lock_guard<std::mutex> gd(mmutex_);
                    conns_.erase(aa.first);               // 从EventLoop的map中删除超时的conn。
                }
                timercallback_(aa.first);             // 从TcpServer的map中删除超时的conn。
            }
        }
    }
}

 // 把Connection对象保存在conns_中。
 void EventLoop::newconnection(spConnection conn)
 {
    std::lock_guard<std::mutex> gd(mmutex_);
    conns_[conn->fd()]=conn;
 }

 // 将被设置为TcpServer::removeconn()
 void EventLoop::settimercallback(std::function<void(int)> fn)
 {
    timercallback_=fn;
 }