/*
 * 程序名：bankserver.cpp，网上银行（BankServer）服务器。
 * 作者：吴从周
*/
#include <signal.h>
#include "BankServer.h"

// 1、设置2和15的信号。
// 2、在信号处理函数中停止主从事件循环和工作线程。
// 3、服务程序主动退出。

BankServer *bankserver;

void Stop(int sig)    // 信号2和15的处理函数，功能是停止服务程序。
{
    printf("sig=%d\n",sig);
    // 调用BankServer::Stop()停止服务。
    bankserver->Stop();
    printf("bankserver已停止。\n");
    delete bankserver;
    printf("delete bankserver。\n");
    exit(0); 
}

int main(int argc,char *argv[])
{
    if (argc != 3) 
    { 
        printf("usage: ./bankserver ip port\n"); 
        printf("example: ./bankserver 192.168.150.128 5085\n\n"); 
        return -1; 
    }

    signal(SIGTERM,Stop);    // 信号15，系统kill或killall命令默认发送的信号。
    signal(SIGINT,Stop);        // 信号2，按Ctrl+C发送的信号。

    bankserver=new BankServer(argv[1],atoi(argv[2]),3,2);
    bankserver->Start();

    return 0;
}
