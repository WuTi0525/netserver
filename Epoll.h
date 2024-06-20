#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <string.h>
#include <sys/epoll.h>
#include <vector>
#include <unistd.h>
#include "Channel.h"

class Channel;

// Epoll类。
class Epoll
{
private:
    static const int MaxEvents=100;                   // epoll_wait()返回事件数组的大小。
    int epollfd_=-1;                                             // epoll句柄，在构造函数中创建。
    epoll_event events_[MaxEvents];                  // 存放poll_wait()返回事件的数组，在构造函数中分配内存。
public:
    Epoll();                                             // 在构造函数中创建了epollfd_。
    ~Epoll();                                          // 在析构函数中关闭epollfd_。

    void updatechannel(Channel *ch);                        // 把channel添加/更新到红黑树上，channel中有fd，也有需要监视的事件。
    void removechannel(Channel *ch);                        // 从红黑树上删除channel。
    std::vector<Channel *> loop(int timeout=-1);   // 运行epoll_wait()，等待事件的发生，已发生的事件用vector容器返回。
};