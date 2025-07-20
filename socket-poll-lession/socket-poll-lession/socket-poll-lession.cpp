#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <vector>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENTS 64

WSAPOLLFD fds[MAX_CLIENTS];
int nfds = 1;
SOCKET listenSock = INVALID_SOCKET;
//发生消息的线程函数
DWORD WINAPI sendMsg(LPVOID lpParam) {
 
    while (true) {
        
        char msg[128];
       fgets(msg, sizeof(msg), stdin);
        for (int i = 0; i < nfds; i++) {
            if (fds[i].fd == listenSock||fds[i].fd==0) {
                continue;
            }
           int result =  send(fds[i].fd, msg, sizeof(msg), 0);
           if (result < 0) {
               printf("send error:%d\n", WSAGetLastError());
               break;
           }

        }
    }
    
    return 0;
}
// 用于发送消息的线程句柄
HANDLE busThread;

int main() {
  
    WSADATA wsa;
    //初始化
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // 创建监听套接字
    listenSock =  socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //初始化
    sockaddr_in addr = { 0 };
    // 设置为网络
    addr.sin_family = AF_INET;
    // 使用0.0.0.0作为ip 将主机字节顺序转换为网络字节顺序
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //使用80作为端口，从主机字节序转换为网络字节序（大端序）
    addr.sin_port = htons(80);
    //绑定socket 跟地址
    bind(listenSock, (SOCKADDR*)&addr, sizeof(addr));
    //监听socket
    listen(listenSock, SOMAXCONN);
    // 创建线程，用于发生消息
    busThread = CreateThread(NULL, 0, sendMsg, NULL, 0, NULL);
    if (busThread == NULL) {
        fprintf(stderr, "CreateThread failed (%d)\n", GetLastError());
        return 1;
    }
    // 初始化 pollfd 数组
   
    //std::vector<WSAPOLLFD> fdvector(MAX_CLIENTS);
     // 初始只有监听套接字
 /*   WSAPOLLFD serverFD;*/
    fds[0].fd = listenSock;
    //POLLRDNORM 普通数据可读
    //POLLWRNORM 普通数据可写
    //POLLRDBAND 带而外数据可读
    //POLLERR 发生错误
    // POLLHUP 连接断开
    // POLLNVAL 无效字节
    // 有新的连接，新的数据过来都是可读时间
    fds[0].events = POLLRDNORM;  // 监视新连接
  
    printf("Server started. Waiting for connections...\n");

    while (1) {
        // 等待事件（阻塞模式）
       //  轮训描述符数组，-1代表阻塞直到轮询得到具体事件，0,立即返回，正整数代表等待超时时间为具体的毫秒数
        int ret = WSAPoll(fds, nfds, -1);
        if (ret == SOCKET_ERROR) {
            printf("WSAPoll error: %d\n", WSAGetLastError());
            Sleep(1000);
            continue;
        }

        // 检查所有套接字
        for (int i = 0; i < nfds; i++) {
            // 监听套接字有新连接
            if (fds[i].fd == listenSock && (fds[i].revents & POLLRDNORM)) {
                SOCKET client = accept(listenSock, NULL, NULL);
                printf("New client connected: %d\n", client);

                // 添加到 poll 数组
                if (nfds < MAX_CLIENTS) {

                    fds[nfds].fd = client;
                    fds[nfds].events = POLLRDNORM; // 监视数据可读
                    nfds++;
                  
                }
                else {
                    closesocket(client);
                }
            }
            // 客户端套接字有数据可读
            else if(fds[i].revents & POLLRDNORM){
                char buf[1024];
                int bytes = recv(fds[i].fd, buf, sizeof(buf), 0);
                if (bytes > 0) {
                    int bytes = recv(fds[i].fd, buf, sizeof(buf), 0);
                    printf(" %s", buf);
                    //send(fds[i].fd, resp, sizeof(resp), 0); // 回显数据
                  
                }
                //else {
                //    closesocket(fds[i].fd);
                //    //将最后一个放到当前位置，覆盖当前的描述符，从而删除描述符
                //    fds[i] = fds[nfds - 1];
                //    nfds--;
                //    i--; // 重新检查当前位置
                //    printf("error");

                //}

            }
            
            
        }

                }
                   
    // 清理
    WSACleanup();
    return 0;
}