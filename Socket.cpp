#include "Socket.h"

// 创建一个非阻塞的socket。
int createnonblocking()
{
    // 创建服务端用于监听的listenfd。
    int listenfd = socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,IPPROTO_TCP);
    if (listenfd < 0)
    {
        printf("%s:%s:%d listen socket create error:%d\n", __FILE__, __FUNCTION__, __LINE__, errno); exit(-1);
    }
    return listenfd;
}

 // 构造函数，传入一个已准备好的fd。
Socket::Socket(int fd):fd_(fd)            
{

}

// 在析构函数中，将关闭fd_。
Socket::~Socket()
{
    ::close(fd_);
}

int Socket::fd() const                              // 返回fd_成员。
{
    return fd_;
}

std::string Socket::ip() const                              // 返回ip_成员。
{
    return ip_;
}

uint16_t Socket::port() const                              // 返回port_成员。
{
    return port_;
}

void Socket::settcpnodelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)); // TCP_NODELAY包含头文件 <netinet/tcp.h>
}

void Socket::setreuseaddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); 
}

void Socket::setreuseport(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)); 
}

void Socket::setkeepalive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)); 
}

void Socket::bind(const InetAddress& servaddr)
{
    if (::bind(fd_,servaddr.addr(),sizeof(sockaddr)) < 0 )
    {
        perror("bind() failed"); close(fd_); exit(-1);
    }

    setipport(servaddr.ip(),servaddr.port());
}

 // 设置ip_和port_成员。
 void Socket::setipport(const std::string &ip,uint16_t port)   
 {
    ip_=ip;
    port_=port;
 }

void Socket::listen(int nn)
{
    if (::listen(fd_,nn) != 0 )        // 在高并发的网络服务器中，第二个参数要大一些。
    {
        perror("listen() failed"); close(fd_); exit(-1);
    }
}

int Socket::accept(InetAddress& clientaddr)
{
    sockaddr_in peeraddr;
    socklen_t len = sizeof(peeraddr);
    int clientfd = accept4(fd_,(sockaddr*)&peeraddr,&len,SOCK_NONBLOCK);

    clientaddr.setaddr(peeraddr);             // 客户端的地址和协议。

    return clientfd;    
}