#include "BankServer.h"

BankServer::BankServer(const std::string &ip,const uint16_t port,int subthreadnum,int workthreadnum)
                   :tcpserver_(ip,port,subthreadnum),threadpool_(workthreadnum,"WORKS")
{
    // 以下代码不是必须的，业务关心什么事件，就指定相应的回调函数。
    tcpserver_.setnewconnectioncb(std::bind(&BankServer::HandleNewConnection, this, std::placeholders::_1));
    tcpserver_.setcloseconnectioncb(std::bind(&BankServer::HandleClose, this, std::placeholders::_1));
    tcpserver_.seterrorconnectioncb(std::bind(&BankServer::HandleError, this, std::placeholders::_1));
    tcpserver_.setonmessagecb(std::bind(&BankServer::HandleMessage, this, std::placeholders::_1, std::placeholders::_2));
    tcpserver_.setsendcompletecb(std::bind(&BankServer::HandleSendComplete, this, std::placeholders::_1));
    // tcpserver_.settimeoutcb(std::bind(&BankServer::HandleTimeOut, this, std::placeholders::_1));
    tcpserver_.setremoveconnectioncb(std::bind(&BankServer::HandleRemove, this, std::placeholders::_1));
}

BankServer::~BankServer()
{

}

// 启动服务。
void BankServer::Start()                
{
    tcpserver_.start();
}

 // 停止服务。
 void BankServer::Stop()
 {
    // 停止工作线程。
    threadpool_.stop();
    printf("工作线程已停止。\n");

    // 停止IO线程（事件循环）。
    tcpserver_.stop();
 }

// 处理新客户端连接请求，在TcpServer类中回调此函数。
void BankServer::HandleNewConnection(spConnection conn)    
{
    // 新客户端连上来时，把用户连接的信息保存到状态机中。
    spUserInfo userinfo(new UserInfo(conn->fd(),conn->ip()));   
    {
        std::lock_guard<std::mutex> gd(mutex_);
        usermap_[conn->fd()]=userinfo;                   // 把用户添加到状态机中。
    }
    printf ("%s 新建连接（ip=%s）.\n",Timestamp::now().tostring().c_str(),conn->ip().c_str());
}

// 关闭客户端的连接，在TcpServer类中回调此函数。 
void BankServer::HandleClose(spConnection conn)  
{
    // 关闭客户端连接的时候，从状态机中删除客户端连接的信息。
    printf ("%s 连接已断开（ip=%s）.\n",Timestamp::now().tostring().c_str(),conn->ip().c_str());
    {
        std::lock_guard<std::mutex> gd(mutex_);
        usermap_.erase(conn->fd());                        // 从状态机中删除用户信息。
    }
}

// 客户端的连接错误，在TcpServer类中回调此函数。
void BankServer::HandleError(spConnection conn)  
{
    HandleClose(conn);
}

// 处理客户端的请求报文，在TcpServer类中回调此函数。
void BankServer::HandleMessage(spConnection conn,std::string& message)     
{
    if (threadpool_.size()==0)
    {
        // 如果没有工作线程，表示在IO线程中计算。
        OnMessage(conn,message);       
    }
    else
    {
        // 把业务添加到线程池的任务队列中，交给工作线程去处理业务。
        threadpool_.addtask(std::bind(&BankServer::OnMessage,this,conn,message));
    }
}

bool getxmlbuffer(const std::string &xmlbuffer,const std::string &fieldname,std::string  &value,const int ilen=0)
{
    std::string start="<"+fieldname+">";            // 数据项开始的标签。
    std::string end="</"+fieldname+">";            // 数据项结束的标签。

    int startp=xmlbuffer.find(start);                     // 在xml中查找数据项开始的标签的位置。
    if (startp==std::string::npos) return false;

    int endp=xmlbuffer.find(end);                       // 在xml中查找数据项结束的标签的位置。
    if (endp==std::string::npos) return false;

    // 从xml中截取数据项的内容。
    int itmplen=endp-startp-start.length();
    if ( (ilen>0) && (ilen<itmplen) ) itmplen=ilen;
    value=xmlbuffer.substr(startp+start.length(),itmplen);

    return true;
}

 // 处理客户端的请求报文，用于添加给线程池。
 void BankServer::OnMessage(spConnection conn,std::string& message)     
 {
    spUserInfo userinfo=usermap_[conn->fd()];    // 从状态机中获取客户端的信息。

    // 解析客户端的请求报文，处理各种业务。
    // <bizcode>00101</bizcode><username>wucz</username><password>123465</password>
    std::string bizcode;                 // 业务代码。
    std::string replaymessage;      // 回应报文。
    getxmlbuffer(message,"bizcode",bizcode);       // 从请求报文中解析出业务代码。

    if (bizcode=="00101")     // 登录业务。
    {
        std::string username,password;
        getxmlbuffer(message,"username",username);    // 解析用户名。
        getxmlbuffer(message,"password",password);     // 解析密码。
        if ( (username=="wucz") && (password=="123465") )         // 假设从数据库或Redis中查询用户名和密码。
        {
            // 用户名和密码正确。
            replaymessage="<bizcode>00102</bizcode><retcode>0</retcode><message>ok</message>";
            userinfo->setLogin(true);               // 设置用户的登录状态为true。
        }
        else
        {
            // 用户名和密码不正确。
            replaymessage="<bizcode>00102</bizcode><retcode>-1</retcode><message>用户名或密码不正确。</message>";
        }
    }
    else if (bizcode=="00201")   // 查询余额业务。
    {
        if (userinfo->Login()==true)
        {
            // 把用户的余额从数据库或Redis中查询出来。
            replaymessage="<bizcode>00202</bizcode><retcode>0</retcode><message>5088.80</message>";
        }
        else
        {
            replaymessage="<bizcode>00202</bizcode><retcode>-1</retcode><message>用户未登录。</message>";
        }
    }
    else if (bizcode=="00901")   // 注销业务。
    {
        if (userinfo->Login()==true)
        {
            replaymessage="<bizcode>00902</bizcode><retcode>0</retcode><message>ok</message>";
            userinfo->setLogin(false);               // 设置用户的登录状态为false。
        }
        else
        {
            replaymessage="<bizcode>00902</bizcode><retcode>-1</retcode><message>用户未登录。</message>";
        }
    }
    else if (bizcode=="00001")   // 心跳。
    {
        if (userinfo->Login()==true)
        {
            replaymessage="<bizcode>00002</bizcode><retcode>0</retcode><message>ok</message>";
        }
        else
        {
            replaymessage="<bizcode>00002</bizcode><retcode>-1</retcode><message>用户未登录。</message>";
        }
    }

    conn->send(replaymessage.data(),replaymessage.size());   // 把数据发送出去。 
 }

// 数据发送完成后，在TcpServer类中回调此函数。
void BankServer::HandleSendComplete(spConnection conn)     
{
    // std::cout << "Message send complete." << std::endl;

    // 根据业务的需求，在这里可以增加其它的代码。
}

/*
// epoll_wait()超时，在TcpServer类中回调此函数。
void BankServer::HandleTimeOut(EventLoop *loop)         
{
    std::cout << "BankServer timeout." << std::endl;

    // 根据业务的需求，在这里可以增加其它的代码。
}
*/

void BankServer::HandleRemove(int fd)                       // 客户端的连接超时，在TcpServer类中回调此函数。
 {
    printf("fd(%d) 已超时。\n",fd);

    std::lock_guard<std::mutex> gd(mutex_);
    usermap_.erase(fd);                                      // 从状态机中删除用户信息。
 }