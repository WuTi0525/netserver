#include "TcpServer.h"

TcpServer::TcpServer(const std::string &ip,const uint16_t port,int threadnum)
                 :threadnum_(threadnum),mainloop_(new EventLoop(true)), 
                  acceptor_(mainloop_.get(),ip,port),threadpool_(threadnum_,"IO")
{
    // 设置epoll_wait()超时的回调函数。
    mainloop_->setepolltimeoutcallback(std::bind(&TcpServer::epolltimeout,this,std::placeholders::_1));   

    // 设置处理新客户端连接请求的回调函数。
    acceptor_.setnewconnectioncb(std::bind(&TcpServer::newconnection,this,std::placeholders::_1));

    // 创建从事件循环。
    for (int ii=0;ii<threadnum_;ii++)
    {
        subloops_.emplace_back(new EventLoop(false,5,10));              // 创建从事件循环，存入subloops_容器中。
        subloops_[ii]->setepolltimeoutcallback(std::bind(&TcpServer::epolltimeout,this,std::placeholders::_1));   // 设置timeout超时的回调函数。
        subloops_[ii]->settimercallback(std::bind(&TcpServer::removeconn,this,std::placeholders::_1));   // 设置清理空闲TCP连接的回调函数。
        threadpool_.addtask(std::bind(&EventLoop::run,subloops_[ii].get()));    // 在线程池中运行从事件循环。
    }
}

TcpServer::~TcpServer()
{
}

// 运行事件循环。
void TcpServer::start()          
{
    mainloop_->run();
}

 // 停止IO线程和事件循环。
 void TcpServer::stop()          
 {
    // 停止主事件循环。
    mainloop_->stop();
    printf("主事件循环已停止。\n");

    // 停止从事件循环。
    for (int ii=0;ii<threadnum_;ii++)
    {
        subloops_[ii]->stop();
    }
    printf("从事件循环已停止。\n");

    // 停止IO线程。
    threadpool_.stop();
    printf("IO线程池停止。\n");
 }


// 处理新客户端连接请求。
void TcpServer::newconnection(std::unique_ptr<Socket> clientsock)
{
    // 把新建的conn分配给从事件循环。
    spConnection conn(new Connection(subloops_[clientsock->fd()%threadnum_].get(),std::move(clientsock)));   
    conn->setclosecallback(std::bind(&TcpServer::closeconnection,this,std::placeholders::_1));
    conn->seterrorcallback(std::bind(&TcpServer::errorconnection,this,std::placeholders::_1));
    conn->setonmessagecallback(std::bind(&TcpServer::onmessage,this,std::placeholders::_1,std::placeholders::_2));
    conn->setsendcompletecallback(std::bind(&TcpServer::sendcomplete,this,std::placeholders::_1));
    //conn->enablereading();

    // printf ("new connection(fd=%d,ip=%s,port=%d) ok.\n",conn->fd(),conn->ip().c_str(),conn->port());

    {
        std::lock_guard<std::mutex> gd(mmutex_);
        conns_[conn->fd()]=conn;            // 把conn存放到TcpSever的map容器中。
    }
    subloops_[conn->fd()%threadnum_]->newconnection(conn);       // 把conn存放到EventLoop的map容器中。

    if (newconnectioncb_) newconnectioncb_(conn);             // 回调上层业务类的HandleNewConnection()。
}

 // 关闭客户端的连接，在Connection类中回调此函数。 
 void TcpServer::closeconnection(spConnection conn)
 {
    if (closeconnectioncb_) closeconnectioncb_(conn);       // 回调上层业务类的HandleClose()。

    // printf("client(fd=%d) disconnected.\n",conn->fd());
    {
         std::lock_guard<std::mutex> gd(mmutex_);
        conns_.erase(conn->fd());        // 从map中删除conn。
    }
 }

// 客户端的连接错误，在Connection类中回调此函数。
void TcpServer::errorconnection(spConnection conn)
{
    if (errorconnectioncb_) errorconnectioncb_(conn);     // 回调上层业务类的HandleError()。

    // printf("client(fd=%d) error.\n",conn->fd());
    {
         std::lock_guard<std::mutex> gd(mmutex_);
        conns_.erase(conn->fd());      // 从map中删除conn。
    }
}

// 处理客户端的请求报文，在Connection类中回调此函数。
void TcpServer::onmessage(spConnection conn,std::string& message)
{
    if (onmessagecb_) onmessagecb_(conn,message);     // 回调上层业务类的HandleMessage()。
}

// 数据发送完成后，在Connection类中回调此函数。
void TcpServer::sendcomplete(spConnection conn)     
{
    // printf("send complete.\n");

    if (sendcompletecb_) sendcompletecb_(conn);     // 回调上层业务类的HandleSendComplete()。
}

// epoll_wait()超时，在EventLoop类中回调此函数。
void TcpServer::epolltimeout(EventLoop *loop)         
{
    // printf("epoll_wait() timeout.\n");

    if (timeoutcb_)  timeoutcb_(loop);           // 回调上层业务类的HandleTimeOut()。
}

void TcpServer::setnewconnectioncb(std::function<void(spConnection)> fn)
{
    newconnectioncb_=fn;
}

void TcpServer::setcloseconnectioncb(std::function<void(spConnection)> fn)
{
    closeconnectioncb_=fn;
}

void TcpServer::seterrorconnectioncb(std::function<void(spConnection)> fn)
{
    errorconnectioncb_=fn;
}

void TcpServer::setonmessagecb(std::function<void(spConnection,std::string &message)> fn)
{
    onmessagecb_=fn;
}

void TcpServer::setsendcompletecb(std::function<void(spConnection)> fn)
{
    sendcompletecb_=fn;
}

void TcpServer::settimeoutcb(std::function<void(EventLoop*)> fn)
{
    timeoutcb_=fn;
}

// 删除conns_中的Connection对象，在EventLoop::handletimer()中将回调此函数。
void TcpServer::removeconn(int fd)                 
{
    {
        std::lock_guard<std::mutex> gd(mmutex_);
        conns_.erase(fd);          // 从map中删除conn。
    }

    if (removeconnectioncb_) removeconnectioncb_(fd);
}

void TcpServer::setremoveconnectioncb(std::function<void(int)> fn)
{
    removeconnectioncb_=fn;
}