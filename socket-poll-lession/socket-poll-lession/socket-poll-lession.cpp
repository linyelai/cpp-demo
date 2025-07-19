#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <vector>
#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENTS 64

int main() {

    WSADATA wsa;
    //初始化
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // 创建监听套接字
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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

    // 初始化 pollfd 数组
    WSAPOLLFD fds[MAX_CLIENTS];
    //std::vector<WSAPOLLFD> fdvector(MAX_CLIENTS);
    int nfds = 1;  // 初始只有监听套接字
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
            break;
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
            else{
                char buf[1024];
                int bytes = recv(fds[i].fd, buf, sizeof(buf), 0);
                char resp[] = "HTTP/1.1 200 OK\r\nContent-Type:text/application;charset=utf-8\r\n\r\nHello, world!";
                printf("resp:%i", sizeof(resp));
                if (bytes > 0) {
                    printf("%s", buf);
                    send(fds[i].fd, resp, sizeof(resp), 0); // 回显数据
                  
                }
                    closesocket(fds[i].fd);
                    //将最后一个放到当前位置，覆盖当前的描述符，从而删除描述符
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--; // 重新检查当前位置
                
            } 
         
        }
    }

    // 清理
    WSACleanup();
    return 0;
}