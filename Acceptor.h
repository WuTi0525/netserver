#pragma once
#include <functional>
#include "Socket.h"
#include "InetAddress.h"
#include "Channel.h"
#include "EventLoop.h"
#include <memory>

class Acceptor
{
private:
    EventLoop* loop_;               // Acceptor对应的事件循环，在构造函数中传入。 
    Socket servsock_;                // 服务端用于监听的socket，在构造函数中创建。
    Channel acceptchannel_;    // Acceptor对应的channel，在构造函数中创建。
    std::function<void(std::unique_ptr<Socket>)> newconnectioncb_;   // 处理新客户端连接请求的回调函数，将指向TcpServer::newconnection()

public:
    Acceptor(EventLoop* loop,const std::string &ip,const uint16_t port);
    ~Acceptor();

    void newconnection();    // 处理新客户端连接请求。

    // 设置处理新客户端连接请求的回调函数，将在创建Acceptor对象的时候（TcpServer类的构造函数中）设置。
    void setnewconnectioncb(std::function<void(std::unique_ptr<Socket>)> fn);       
}; 
